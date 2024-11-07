#include <zmq.hpp>
#include <atomic>
#include <thread>
#include <iostream>
#include <string>
#include <type_traits>

// Interface with a pure virtual to_string function
class IStringify {
public:
    virtual ~IStringify() = default;

    virtual void from_string(const std::string& str) = 0;
    virtual std::string to_string() const = 0;
};

template <typename T>
class LockFreeQueue
{
    static_assert(std::is_base_of<IStringify, T>::value, "T must inherit from IStringify");

public:
    LockFreeQueue(std::string name="test", int timeout=-1) : 
        context(1), 
        push_socket(context, ZMQ_PUSH), 
        pull_socket(context, ZMQ_PULL) 
    {
        push_socket.bind("inproc://lfq_" + name);
        pull_socket.connect("inproc://lfq_" + name);
        pull_socket.setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    }

    ~LockFreeQueue() {}

    void enqueue(T value)
    {
        std::string message = value.to_string();
        zmq::message_t zmq_message(message.size());
        memcpy(zmq_message.data(), message.c_str(), message.size());
        push_socket.send(zmq_message);
    }

    bool dequeue(T& value)
    {
        zmq::message_t zmq_message;
        if (!pull_socket.recv(&zmq_message)) {
            return false;
        };
        std::string message(static_cast<char*>(zmq_message.data()), zmq_message.size());
        value.from_string(message);
        return true;
    }
private:
    zmq::context_t context;
    zmq::socket_t push_socket;
    zmq::socket_t pull_socket;
};