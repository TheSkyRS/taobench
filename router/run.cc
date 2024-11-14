#include <thread>
#include <chrono>

# include "router.h"

using namespace benchmark;

ZmqRouter router;
typedef std::vector<std::string> StrArray;

int main() {
    std::string dest_host = "128.110.217.67";
    for (size_t i = 0; i < zmq_read_ports.size(); i++) {
        router.set_rule<MemcacheRequest>(zmq_read_ports[i], 
            StrArray{dest_host}, StrArray{zmq_read_ports[i]});
    }
    for (size_t i = 0; i < zmq_read_txn_ports.size(); i++) {
        router.set_rule<MemcacheRequest>(zmq_read_txn_ports[i], 
            StrArray{dest_host}, StrArray{zmq_read_txn_ports[i]});
    }
    for (size_t i = 0; i < zmq_write_ports.size(); i++) {
        router.set_rule<MemcacheRequest>(zmq_write_ports[i], 
            StrArray{dest_host}, StrArray{zmq_write_ports[i]});
    }
    std::this_thread::sleep_for(std::chrono::hours(1000000));
    return 0;
}

// Compile with: g++ run.cc -pthread -lzmq -I../src
