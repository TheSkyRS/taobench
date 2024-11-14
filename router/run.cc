#include <thread>
#include <chrono>
#include <random>

# include "router.h"

using namespace benchmark;

ZmqRouter router;
typedef std::vector<std::string> StrArray;

int route_rule(MemcacheRequest* req) {
    std::random_device rd;
    std::mt19937 gen{rd()};
    std::uniform_int_distribution<int> dist(0, 127);
    return dist(gen);
}

int main() {
    std::string dest_host = "128.110.217.67";
    for (size_t i = 0; i < zmq_read_ports.size(); i++) {
        router.set_rule<MemcacheRequest>(zmq_read_ports[i], 
            StrArray(zmq_read_ports.size(), dest_host), 
            zmq_read_ports, "tcp", route_rule);
    }
    for (size_t i = 0; i < zmq_read_txn_ports.size(); i++) {
        router.set_rule<MemcacheRequest>(zmq_read_txn_ports[i], 
            StrArray(zmq_read_txn_ports.size(), dest_host), 
            zmq_read_txn_ports, "tcp", route_rule);
    }
    for (size_t i = 0; i < zmq_write_ports.size(); i++) {
        router.set_rule<MemcacheRequest>(zmq_write_ports[i], 
            StrArray(zmq_write_ports.size(), dest_host), 
            zmq_write_ports, "tcp", route_rule);
    }
    std::this_thread::sleep_for(std::chrono::hours(1000000));
    return 0;
}

// Compile with: g++ run.cc -pthread -lzmq -I../src
