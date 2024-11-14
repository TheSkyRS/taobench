#include <thread>
#include <chrono>

# include "router.h"
# include "../memcache/memcache_struct.h"

using namespace benchmark;

ZmqRouter router;

int main() {
    std::string dest_host = "128.110.217.67";
    for (size_t i = 0; i < zmq_read_ports.size(); i++) {
        router.set_rule(zmq_read_ports[i], std::vector<std::string>{dest_host}, 
                        std::vector<std::string>{zmq_read_ports[i]});
    }
    for (size_t i = 0; i < zmq_read_txn_ports.size(); i++) {
        router.set_rule(zmq_read_txn_ports[i], std::vector<std::string>{dest_host}, 
                        std::vector<std::string>{zmq_read_txn_ports[i]});
    }
    for (size_t i = 0; i < zmq_write_ports.size(); i++) {
        router.set_rule(zmq_write_ports[i], std::vector<std::string>{dest_host}, 
                        std::vector<std::string>{zmq_write_ports[i]});
    }
    std::this_thread::sleep_for(std::chrono::hours(1000000));
    return 0;
}

// Compile with: g++ run.cc -pthread -lzmq -I../src
