#include <zmq.hpp>
#include <msgpack.hpp>
#include <iostream>
#include <thread>
#include <string>
#include <vector>

#include "../memcache/zmq_utils.h"

struct TestData {
    int id;
    std::string name;
    MSGPACK_DEFINE(id, name);
};

int main() {
    zmq::context_t context(1);
    WebPublish<TestData> publisher(&context);
    WebSubscribe<TestData> subscriber(&context);

    std::thread pub_thread([&]() {
        for (int i = 0; i < 5; ++i) {
            TestData data = {i, "Message " + std::to_string(i)};
            publisher.push(data);
            std::cout << "Published: { id: " << data.id
                      << ", name: " << data.name << " }" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    std::thread sub_thread([&]() {
        TestData data;
        while (true) {
            if (subscriber.poll(data)) {
                std::cout << "Received: { id: " << data.id
                          << ", name: " << data.name << " }" << std::endl;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    });

    pub_thread.join();
    sub_thread.detach();

    return 0;
}
