#include <zmq.hpp>
#include <iostream>
#include <thread>
#include <chrono>

// Producer thread function
void producer(zmq::context_t &context) {
    zmq::socket_t socket(context, ZMQ_PUSH);
    socket.bind("inproc://example");

    for (int i = 0; i < 5; ++i) {
        std::string message = "Message " + std::to_string(i);
        zmq::message_t zmq_message(message.size());
        memcpy(zmq_message.data(), message.c_str(), message.size());
        socket.send(zmq_message);

        std::cout << "Produced: " << message << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  // Simulate work
    }
}

// Consumer thread function
void consumer(zmq::context_t &context) {
    zmq::socket_t socket(context, ZMQ_PULL);
    socket.connect("inproc://example");

    for (int i = 0; i < 5; ++i) {
        zmq::message_t zmq_message;
        socket.recv(&zmq_message);  // Compatible with ZeroMQ 3
        std::string message(static_cast<char*>(zmq_message.data()), zmq_message.size());

        std::cout << "Consumed: " << message << std::endl;
    }
}

int main() {
    zmq::context_t context(1);

    // Start producer and consumer threads
    std::thread producer_thread(producer, std::ref(context));
    std::thread consumer_thread(consumer, std::ref(context));

    // Wait for both threads to finish
    producer_thread.join();
    consumer_thread.join();

    return 0;
}
