#include <thread>
#include <chrono>
#include <random>

#include "router.h"

using namespace benchmark;

ZmqRouter router;
typedef std::vector<std::string> StrArray;

class RandomFunc: public RouterFunc<MemcacheRequest> {
public:
    RandomFunc(): gen(std::random_device{}()), dist(0, 127),
        read_ofs(0), 
        read_txn_ofs(read_ofs + zmq_read_ports.size()),
        write_ofs(read_txn_ofs + zmq_read_txn_ports.size()), 
        total_size(write_ofs + zmq_write_ports.size()) {}

    int index(const MemcacheRequest& value) override {
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
    std::string dest_host = "127.0.0.1";
    if(argc > 1) {
        dest_host = argv[1];
    }
    
    const size_t size = zmq_read_ports.size() + zmq_read_txn_ports.size() + zmq_write_ports.size();
    std::vector<std::string> hosts(size, dest_host);
    std::vector<std::string> ports(zmq_read_ports);
    ports.insert(ports.end(), zmq_read_txn_ports.begin(), zmq_read_txn_ports.end());
    ports.insert(ports.end(), zmq_write_ports.begin(), zmq_write_ports.end());

    for (const auto& router_port: zmq_router_ports) {
        router.set_rule<MemcacheRequest>(router_port, hosts, ports, "tcp", new RandomFunc());
    }
    std::this_thread::sleep_for(std::chrono::hours(1000000));
    return 0;
}

// Compile with: g++ run.cc -pthread -lzmq -I../src