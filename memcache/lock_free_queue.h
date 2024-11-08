#include <zmq.hpp>
#include <atomic>
#include <thread>
#include <iostream>
#include <string>
#include <msgpack.hpp>
#include <queue>
#include <cassert>
#include <atomic>

#ifdef SIMPLE_QUEUE

template <typename T>
class WebQueue
{
public:
    WebQueue(): m_head(new Node), m_tail(m_head.load()) {}

    ~WebQueue()
    {
        while (Node* const old_head = m_head)
        {
            m_head = old_head->next;
            delete old_head;
        }
    }

    void init(zmq::context_t& context, std::string name="test", int timeout=-1) {}

    void enqueue(T value)
    {
        Node* const new_node = new Node(value);
        Node* old_tail = m_tail.exchange(new_node);
        old_tail->next = new_node;
    }
    bool dequeue(T& value)
    {
        Node* old_head = m_head;
        Node* new_head;
        do
        {
            if (old_head->next == nullptr)
            {
                return false;
            }
            new_head = old_head->next;
        } while (!m_head.compare_exchange_weak(old_head, new_head));
        value = new_head->value;
        delete old_head;
        return true;
    }
private:
    struct Node
    {
        T value;
        Node* next;
        Node() : next(nullptr) {}
        Node(T value) : value(value), next(nullptr) {}
    };
    std::atomic<Node*> m_head;
    std::atomic<Node*> m_tail;
};

#else

template <typename T>
class WebQueue
{
public:
    WebQueue() {}

    ~WebQueue() {
        if (push_socket) delete push_socket;
        if (pull_socket) delete pull_socket;
    }

    void init(zmq::context_t& context, std::string name="test", int timeout=-1)
    {
        push_socket = new zmq::socket_t(context, ZMQ_PUSH);
        pull_socket = new zmq::socket_t(context, ZMQ_PULL);
        push_socket->bind("inproc://lfq_" + name);
        pull_socket->connect("inproc://lfq_" + name);
        pull_socket->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
        std::cout << "creating ZeroMQ" << std::endl;
    }

    void enqueue(T value)
    {
        msgpack::sbuffer sbuf;
        msgpack::pack(sbuf, value);

        zmq::message_t message(sbuf.size());
        memcpy(message.data(), sbuf.data(), sbuf.size());
        push_socket->send(message);
    }

    bool dequeue(T& value)
    {
        zmq::message_t message;
        if (!pull_socket->recv(&message)) {
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
    zmq::socket_t* push_socket = nullptr;
    zmq::socket_t* pull_socket = nullptr;
};

#endif