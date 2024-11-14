#include <zmq.hpp>
#include <iostream>
#include <string>
#include <future>
#include <vector>
#include <random>

#include "../memcache/memcache_struct.h"
#include "../memcache/lock_free_queue.h"

class ZmqRouter {
public:
    ZmqRouter() {}

    template <typename T>
    void set_rule(std::string listen_port, std::vector<std::string> dest_hosts, 
        std::vector<std::string> dest_ports) 
    {
        thread_pool_.push_back(std::async(std::launch::async,
            run<T>, listen_port, dest_hosts, dest_ports, "tcp"
        ));
    }

    template <typename T>
    static void run(std::string listen_port, std::vector<std::string> dest_hosts, 
        std::vector<std::string> dest_ports, std::string protocol) 
    {
        std::random_device rd;
        std::mt19937 gen{rd()};
        std::uniform_int_distribution<int> dist(0, 127);

        zmq::context_t ctx{1};
        WebQueuePull<T> router_queue(&ctx, listen_port, "*", protocol);
        WebQueuePush<T> dealer_queue(&ctx);

        for (int i = 0; i < dest_hosts.size(); i++) {
            dealer_queue.connect(dest_ports[i], dest_hosts[i], protocol);
        }

        while (true) {
            T value;
            router_queue.dequeue(value);
            dealer_queue.enqueue(value, dist(gen));
        }
    }

private:
    std::vector<std::future<void>> thread_pool_;
};