#include <zmq.hpp>
#include <iostream>
#include <string>
#include <future>
#include <vector>

#include "../memcache/memcache_struct.h"
#include "../memcache/zmq_utils.h"

using namespace benchmark;

class RouterFunc {
public:
    virtual int index(const MemcacheData& value) = 0;
};

class ZmqRouter {
    typedef std::vector<std::string> StrArray;

public:
    ZmqRouter(std::string self_addr): self_addr_(self_addr) {}

    void set_rule(std::string listen_port, std::string resp_port, StrArray dest_hosts, 
                  StrArray dest_ports, std::string protocol, RouterFunc* func) 
    {
        FullAddr resp_addr{self_addr_, resp_port};
        thread_pool_.push_back(std::async(std::launch::async, send, 
            listen_port, resp_addr, dest_hosts, dest_ports, protocol, func
        ));
        thread_pool_.push_back(std::async(std::launch::async, recv, 
            resp_port, protocol
        ));
    }

private:
    static void send(std::string listen_port, FullAddr resp_addr, StrArray dest_hosts, 
                    StrArray dest_ports, std::string protocol, RouterFunc* func) 
    {
        zmq::context_t ctx{1};
        WebQueuePull<MemcacheData> router_queue(&ctx, listen_port, "*", protocol);
        WebQueuePush<MemcacheData> dealer_queue(&ctx);

        for (int i = 0; i < dest_hosts.size(); i++) {
            dealer_queue.connect(dest_ports[i], dest_hosts[i], protocol);
        }

        MemcacheData value;
        while (true) {
            if (!router_queue.dequeue(value)) {
                continue;
            }
            value.resp_addr.push_back(resp_addr);
            dealer_queue.enqueue(value, func->index(value));
        }
    }

    static void recv(std::string resp_port, std::string protocol) {
        zmq::context_t ctx{1};
        WebQueuePull<MemcacheData> resp_queue(&ctx, resp_port, "*", protocol);
        std::unordered_map<FullAddr, WebQueuePush<MemcacheData>*, FullAddrHash> ans_queue;

        MemcacheData value;
        while (true) {
            if (!resp_queue.dequeue(value)) {
                continue;
            }
            FullAddr& addr = value.resp_addr.back();
            if (ans_queue.find(addr) == ans_queue.end()) {
                ans_queue[addr] = new WebQueuePush<MemcacheData>(&ctx);
                ans_queue[addr]->connect(addr.port, addr.addr);
            }
            ans_queue[addr]->enqueue(value);
        }
    }

    const std::string self_addr_;
    std::vector<std::future<void>> thread_pool_;
};