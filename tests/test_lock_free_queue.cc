#include "../memcache/lock_free_queue.h"

class Integer: public IStringify {
public:
    int value;

    Integer(int value_) {
        value = value_;
    }

    void from_string(const std::string& str) override {
        value = std::stoi(str);
    }

    std::string to_string() const override {
        return std::to_string(value);
    }
};

int main()
{
    LockFreeQueue<Integer> queue;
    std::thread t1([&queue]()
    {
        for (int i = 0; i < 10; ++i)
        {
            queue.enqueue(Integer(i));
        }
    });
    std::thread t2([&queue]()
    {
        Integer value(0);
        while (value.value < 9)
        {
            if (queue.dequeue(value))
            {
                std::cout << "Dequeued value: " << value.to_string() << std::endl;
            }
        }
    });
    t1.join();
    t2.join();
    return 0;
}