#include <zmq.hpp>
#include <atomic>
#include <thread>
#include <iostream>
#include <string>
#include <msgpack.hpp>
#include <queue>
#include <cassert>
#include <vector>

template <typename T>
class WebQueuePush
{
public:
    WebQueuePush(zmq::context_t* ctx): ctx_(ctx) {}

    ~WebQueuePush() {}

    void connect(std::string port="6000", std::string host="127.0.0.1", 
        std::string protocol="tcp")
    {
        push_sockets.push_back(zmq::socket_t(*ctx_, ZMQ_PUSH));
        push_sockets.back().connect(protocol + "://" + host + ":" + port);
        std::cout << "ZeroMQ connecting to " << host << ":" << port << std::endl;
    }

    void enqueue(T value, int idx=0)
    {
        idx = idx % push_sockets.size();
        msgpack::sbuffer sbuf;
        msgpack::pack(sbuf, value);

        zmq::message_t message(sbuf.size());
        memcpy(message.data(), sbuf.data(), sbuf.size());
        push_sockets[idx].send(message);
    }
private:
    zmq::context_t* ctx_;
    std::vector<zmq::socket_t> push_sockets;
};

template <typename T>
class WebQueuePull
{
public:
    WebQueuePull(zmq::context_t* ctx, std::string port="6000", 
        std::string host="127.0.0.1", std::string protocol="tcp"): 
        ctx_(ctx), pull_socket(*ctx_, ZMQ_PULL), port_(port)
    {
        pull_socket.bind(protocol + "://" + host + ":" + port);
        std::cout << "ZeroMQ listening on " << host << ":" << port << std::endl;
    }

    void setup(int timeout=-1, int capacity=1000) {
        pull_socket.setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        pull_socket.setsockopt(ZMQ_SNDHWM, &capacity, sizeof(capacity));
    }

    ~WebQueuePull() {}

    bool dequeue(T& value)
    {
        zmq::message_t message;
        if (!pull_socket.recv(&message)) {
            return false;
        };
        msgpack::object_handle handle = msgpack::unpack(
            static_cast<const char*>(message.data()), message.size()
        );
        msgpack::object deserialized = handle.get();
        deserialized.convert(value);
        return true;
    }
private:
    zmq::context_t* ctx_;
    zmq::socket_t pull_socket;

    std::string port_;
};

template <typename T>
class WebQueue
{
public:
    WebQueue(std::string port="6000", int timeout=-1):
        push(new zmq::context_t(1)), pull(new zmq::context_t(1), port, timeout) 
    {
        push.connect(port);
    }

    ~WebQueue() {}

    void enqueue(T value) {
        push.enqueue(value);
    }

    bool dequeue(T& value) {
        return pull.dequeue(value);
    } 

private:
    WebQueuePush<T> push;
    WebQueuePull<T> pull;
};
