#include <zmq.hpp>
#include <iostream>
#include <string>
#include <future>
#include <vector>

#include "../memcache/memcache_struct.h"
#include "../memcache/lock_free_queue.h"

class ZmqRouter {
    template <typename T>
    using Func = int(*)(T*);
    typedef std::vector<std::string> StrArray;

public:
    ZmqRouter() {}

    template <typename T>
    void set_rule(std::string listen_port, StrArray dest_hosts, 
        StrArray dest_ports, std::string protocol, Func<T> func) 
    {
        thread_pool_.push_back(std::async(std::launch::async,
            run<T>, listen_port, dest_hosts, dest_ports, protocol, func
        ));
    }

    template <typename T>
    static void run(std::string listen_port, StrArray dest_hosts, 
        StrArray dest_ports, std::string protocol, Func<T> func) 
    {
        zmq::context_t ctx{1};
        WebQueuePull<T> router_queue(&ctx, listen_port, "*", protocol);
        WebQueuePush<T> dealer_queue(&ctx);

        for (int i = 0; i < dest_hosts.size(); i++) {
            dealer_queue.connect(dest_ports[i], dest_hosts[i], protocol);
        }

        while (true) {
            T value;
            router_queue.dequeue(value);
            dealer_queue.enqueue(value, func(&value));
        }
    }

private:
    std::vector<std::future<void>> thread_pool_;
};