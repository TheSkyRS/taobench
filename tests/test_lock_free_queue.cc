#include "../memcache/lock_free_queue.h"

class Integer {
public:
    int value;

    Integer(int value_) {
        value = value_;
    }

    MSGPACK_DEFINE(value);
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
                std::cout << "Dequeued value: " << value.value << std::endl;
            }
        }
    });
    t1.join();
    t2.join();
    return 0;
}