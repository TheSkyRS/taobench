#include "../memcache/lock_free_queue.h"

int main()
{
    LockFreeQueue<int> queue;
    std::thread t1([&queue]()
    {
        for (int i = 0; i < 10; ++i)
        {
            queue.enqueue(i);
        }
    });
    std::thread t2([&queue]()
    {
        int value = 0;
        while (value < 9)
        {
            if (queue.dequeue(value))
            {
                std::cout << "Dequeued value: " << value << std::endl;
            }
        }
    });
    t1.join();
    t2.join();
    return 0;
}