#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include "router.h"

using namespace benchmark;

typedef std::vector<std::string> StrArray;

class RandomFunc: public RouterFunc {
public:
    RandomFunc(): gen(std::random_device{}()), dist(0, 127),
        read_ofs(0), 
        read_txn_ofs(read_ofs + zmq_read_ports.size()),
        write_ofs(read_txn_ofs + zmq_read_txn_ports.size()), 
        total_size(write_ofs + zmq_write_ports.size()) {}

    int index(const MemcacheData& value) override {
        if (value.read_only) {
            if (!value.txn_op) {
                return (dist(gen) % (read_txn_ofs - read_ofs)) + read_ofs;
            }
            return (dist(gen) % (write_ofs - read_txn_ofs)) + read_txn_ofs;
        }
        return (dist(gen) % (total_size - write_ofs)) + write_ofs;
    }

private:
    std::mt19937 gen;
    std::uniform_int_distribution<int> dist;
    const int read_ofs, read_txn_ofs, write_ofs, total_size;
};

int main(int argc, char* argv[]) {
    std::string self_addr = "127.0.0.1";
    std::vector<std::string> dest_hosts;
    if (argc > 1) {
        self_addr = argv[1];
        for (int i = 2; i < argc; i ++) {
            dest_hosts.push_back(argv[i]);
        }
    }
    if (dest_hosts.empty()) {
        dest_hosts.push_back("127.0.0.1");
    }

    std::vector<std::string> hosts, ports;
    for (size_t hid = 0; hid < dest_hosts.size(); hid ++){
        for (size_t i = 0; i < zmq_read_ports.size(); i ++) {
            hosts.push_back(dest_hosts[hid]);
            ports.push_back(zmq_read_ports[i]);
        }
        for (size_t i = 0; i < zmq_read_txn_ports.size(); i ++) {
            hosts.push_back(dest_hosts[hid]);
            ports.push_back(zmq_read_txn_ports[i]);
        }
        for (size_t i = 0; i < zmq_write_ports.size(); i ++) {
            hosts.push_back(dest_hosts[hid]);
            ports.push_back(zmq_write_ports[i]);
        }
    }
    
    ZmqRouter router(self_addr);
    for (int i=0; i < zmq_router_ports.size(); i++) {
        router.set_rule(zmq_router_ports[i], zmq_router_rports[i], 
                        hosts, ports, "tcp", new RandomFunc());
    }
    std::this_thread::sleep_for(std::chrono::hours(1000000));
    return 0;
}

// Compile with: g++ -o router run.cc -pthread -lzmq -I../src
