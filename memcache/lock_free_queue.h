#include <atomic>
#include <thread>
#include <iostream>
template <typename T>
class LockFreeQueue
{
public:
    LockFreeQueue() : m_head(new Node), m_tail(m_head.load()) {}
    ~LockFreeQueue()
    {
        while (Node* const old_head = m_head)
        {
            m_head = old_head->next;
            delete old_head;
        }
    }
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