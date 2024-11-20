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
class WebPublish
{
public:
    WebPublish(zmq::context_t* ctx, std::string port="6998", 
        std::string host="127.0.0.1", std::string protocol="tcp"): 
        ctx_(ctx), publish_socket(*ctx_, ZMQ_PUB)
    {
        publish_socket.bind(protocol + "://" + host + ":" + port);
        std::cout << "ZeroMQ publish thro " << host << ":" << port << std::endl;
    }

    void push(const T& value) 
    {
        msgpack::sbuffer sbuf;
        msgpack::pack(sbuf, value);

        zmq::message_t message(sbuf.size());
        memcpy(message.data(), sbuf.data(), sbuf.size());
        publish_socket.send(message);
    }

private:
    zmq::context_t* ctx_;
    zmq::socket_t publish_socket;
};

template <typename T>
class WebSubscribe
{
public:
    WebSubscribe(zmq::context_t* ctx, std::string port="6998", 
        std::string host="127.0.0.1", std::string protocol="tcp"): 
        ctx_(ctx), subscribe_socket(*ctx_, ZMQ_SUB)
    {
        subscribe_socket.connect(protocol + "://" + host + ":" + port);
        subscribe_socket.setsockopt(ZMQ_SUBSCRIBE, "", 0);
        std::cout << "ZeroMQ subscribe thro " << host << ":" << port << std::endl;
    }

    bool poll(T& value) 
    {
        zmq::message_t message;
        if (!subscribe_socket.recv(&message, ZMQ_DONTWAIT)) {
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
    zmq::socket_t subscribe_socket;
};

template <typename T>
class WebQueuePush
{
public:
    WebQueuePush(zmq::context_t* ctx): ctx_(ctx) {}

    ~WebQueuePush() {}

    void connect(std::string port="6999", std::string host="127.0.0.1", 
        std::string protocol="tcp")
    {
        push_sockets.push_back(zmq::socket_t(*ctx_, ZMQ_PUSH));
        push_sockets.back().connect(protocol + "://" + host + ":" + port);
        std::cout << "ZeroMQ connecting to " << host << ":" << port << std::endl;
    }

    void enqueue(const T& value, int idx=0)
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
    WebQueuePull(zmq::context_t* ctx, std::string port="6999", 
        std::string host="127.0.0.1", std::string protocol="tcp"): 
        ctx_(ctx), pull_socket(*ctx_, ZMQ_PULL)
    {
        pull_socket.bind(protocol + "://" + host + ":" + port);
        std::cout << "ZeroMQ listening on " << host << ":" << port << std::endl;
        setup();
    }

    void setup(int timeout=0, int capacity=1000) {
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
};

template <typename T>
class WebQueue
{
public:
    WebQueue(std::string port="6999", int timeout=-1):
        push(new zmq::context_t(1)), pull(new zmq::context_t(1), port, timeout) 
    {
        push.connect(port);
    }

    ~WebQueue() {}

    void enqueue(const T& value) {
        push.enqueue(value);
    }

    bool dequeue(T& value) {
        return pull.dequeue(value);
    } 

private:
    WebQueuePush<T> push;
    WebQueuePull<T> pull;
};
