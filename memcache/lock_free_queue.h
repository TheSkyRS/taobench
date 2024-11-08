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

    void connect(std::string port="test"){
        push_sockets.push_back(zmq::socket_t(*ctx_, ZMQ_PUSH));
        push_sockets[push_sockets.size()-1].connect("inproc://" + port);
        std::cout << "ZeroMQ connecting to " << port << std::endl;
    }

    void enqueue(T value, int idx=0)
    {
        assert(idx < push_sockets.size());
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
    WebQueuePull(zmq::context_t* ctx, std::string port="test", int timeout=-1): 
        ctx_(ctx), pull_socket(*ctx_, ZMQ_PULL) 
    {
        pull_socket.bind("inproc://" + port);
        pull_socket.setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        std::cout << "ZeroMQ listening on " << port << std::endl;
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
    WebQueue(std::string port="test", int timeout=-1):
        context(1), push(&context), pull(&context, port, timeout) 
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
    zmq::context_t context;
    WebQueuePush<T> push;
    WebQueuePull<T> pull;
};
