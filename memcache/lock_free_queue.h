#include <zmq.hpp>
#include <atomic>
#include <thread>
#include <iostream>
#include <string>
#include <msgpack.hpp>

template <typename T>
class LockFreeQueue
{
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
        msgpack::sbuffer sbuf;
        msgpack::pack(sbuf, value);

        zmq::message_t message(sbuf.size());
        memcpy(message.data(), sbuf.data(), sbuf.size());
        push_socket.send(message);
    }

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
    zmq::context_t context;
    zmq::socket_t push_socket;
    zmq::socket_t pull_socket;
};