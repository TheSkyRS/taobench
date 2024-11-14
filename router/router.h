#include <zmq.hpp>
#include <iostream>
#include <string>
#include <future>
#include <vector>
#include <random>

class ZmqRouter {
public:
    ZmqRouter() {}

    void set_rule(std::string listen_port, std::vector<std::string> dest_hosts, 
        std::vector<std::string> dest_ports) 
    {
        thread_pool_.push_back(std::async(std::launch::async,
            run, listen_port, dest_hosts, dest_ports, "tcp"
        ));
    }

    static void run(std::string listen_port, std::vector<std::string> dest_hosts, 
        std::vector<std::string> dest_ports, std::string protocol) 
    {
        std::random_device rd;
        std::mt19937 gen{rd()};
        std::uniform_int_distribution<int> dist(0, 127);

        zmq::context_t context{1};
        zmq::socket_t router_socket(context, ZMQ_PULL);
        router_socket.bind(protocol + "://*:" + listen_port);
        std::cout << "Router listening on " << listen_port << std::endl;
        std::vector<zmq::socket_t> dealer_sockets;

        for (int i = 0; i < dest_hosts.size(); i++) {
            dealer_sockets.push_back(zmq::socket_t(context, ZMQ_PUSH));
            dealer_sockets.back().connect(protocol + "://" + dest_hosts[i] + ":" + dest_ports[i]);
            std::cout << "Router connecting to " << dest_hosts[i] << ":" << dest_ports[i] << std::endl;
        }

        while (true) {
            zmq::message_t message;
            router_socket.recv(&message);
            int dest_id = dist(gen) % dealer_sockets.size();
            dealer_sockets[dest_id].send(message);
        }
    }

private:
    std::vector<std::future<void>> thread_pool_;
};