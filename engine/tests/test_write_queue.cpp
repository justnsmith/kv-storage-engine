#include "test_framework.h"
#include "write_queue.h"
#include <atomic>
#include <chrono>
#include <ostream>
#include <thread>
#include <vector>

inline std::ostream &operator<<(std::ostream &os, Operation op) {
    switch (op) {
    case Operation::PUT:
        os << "PUT";
        break;
    case Operation::DELETE:
        os << "DELETE";
        break;
    default:
        os << "UNKNOWN";
        break;
    }
    return os;
}

class WriteQueueTest {
  public:
    WriteQueueTest() {
        setUp();
    }

    static void setUp() {
        // Tests will create their own queues as needed
    }
};

bool test_write_queue_creation(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);

    ASSERT_EQ(queue.size(), 0, "New queue should be empty");
    ASSERT_TRUE(!queue.isShutdown(), "New queue should not be shutdown");

    return true;
}

bool test_push_and_pop(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);

    auto future = queue.push(Operation::PUT, "key1", "value1");

    ASSERT_EQ(queue.size(), 1, "Queue should have 1 item");

    auto request = queue.pop();
    ASSERT_TRUE(request.has_value(), "Should pop a request");
    ASSERT_EQ((*request)->op, Operation::PUT, "Operation should be PUT");
    ASSERT_EQ((*request)->key, "key1", "Key should match");
    ASSERT_EQ((*request)->value, "value1", "Value should match");

    ASSERT_EQ(queue.size(), 0, "Queue should be empty after pop");

    return true;
}

bool test_push_multiple_operations(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);

    queue.push(Operation::PUT, "key1", "value1");
    queue.push(Operation::DELETE, "key2", "");
    queue.push(Operation::PUT, "key3", "value3");

    ASSERT_EQ(queue.size(), 3, "Queue should have 3 items");

    auto req1 = queue.pop();
    ASSERT_TRUE(req1.has_value(), "Should pop first request");
    ASSERT_EQ((*req1)->op, Operation::PUT, "First operation should be PUT");
    ASSERT_EQ((*req1)->key, "key1", "First key should be key1");

    auto req2 = queue.pop();
    ASSERT_TRUE(req2.has_value(), "Should pop second request");
    ASSERT_EQ((*req2)->op, Operation::DELETE, "Second operation should be DELETE");
    ASSERT_EQ((*req2)->key, "key2", "Second key should be key2");

    auto req3 = queue.pop();
    ASSERT_TRUE(req3.has_value(), "Should pop third request");
    ASSERT_EQ((*req3)->op, Operation::PUT, "Third operation should be PUT");
    ASSERT_EQ((*req3)->key, "key3", "Third key should be key3");

    ASSERT_EQ(queue.size(), 0, "Queue should be empty");

    return true;
}

bool test_fifo_order(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);

    for (int i = 0; i < 10; i++) {
        queue.push(Operation::PUT, "key" + std::to_string(i), "value" + std::to_string(i));
    }

    for (int i = 0; i < 10; i++) {
        auto req = queue.pop();
        ASSERT_TRUE(req.has_value(), "Should pop request");
        ASSERT_EQ((*req)->key, "key" + std::to_string(i), "Keys should be in FIFO order");
    }

    return true;
}

bool test_pop_batch(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);

    for (int i = 0; i < 10; i++) {
        queue.push(Operation::PUT, "key" + std::to_string(i), "value" + std::to_string(i));
    }

    auto batch = queue.popBatch(5);

    ASSERT_EQ(batch.size(), 5, "Batch should have 5 items");
    ASSERT_EQ(queue.size(), 5, "Queue should have 5 items remaining");

    for (size_t i = 0; i < batch.size(); i++) {
        ASSERT_EQ(batch[i]->key, "key" + std::to_string(i), "Batch items should be in order");
    }

    return true;
}

bool test_pop_batch_all(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);

    for (int i = 0; i < 10; i++) {
        queue.push(Operation::PUT, "key" + std::to_string(i), "value" + std::to_string(i));
    }

    auto batch = queue.popBatch(20);

    ASSERT_EQ(batch.size(), 10, "Batch should have all 10 items");
    ASSERT_EQ(queue.size(), 0, "Queue should be empty");

    return true;
}

bool test_pop_batch_empty_queue(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);

    std::thread popper([&queue]() {
        auto batch = queue.popBatch(10);
        // Should return empty when shutdown
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    queue.shutdown();

    popper.join();

    return true;
}

bool test_capacity_limit(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(5);

    // Fill the queue
    for (int i = 0; i < 5; i++) {
        queue.push(Operation::PUT, "key" + std::to_string(i), "value" + std::to_string(i));
    }

    ASSERT_EQ(queue.size(), 5, "Queue should be at capacity");

    // Try to push one more - should block
    std::atomic<bool> push_completed{false};
    std::thread pusher([&queue, &push_completed]() {
        queue.push(Operation::PUT, "key5", "value5");
        push_completed = true;
    });

    // Wait a bit to ensure push is blocking
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(!push_completed, "Push should be blocked when queue is full");

    // Pop one item to make space
    queue.pop();

    // Wait for push to complete
    pusher.join();
    ASSERT_TRUE(push_completed, "Push should complete after space is available");

    return true;
}

bool test_shutdown(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);

    queue.push(Operation::PUT, "key1", "value1");

    ASSERT_TRUE(!queue.isShutdown(), "Queue should not be shutdown initially");

    queue.shutdown();

    ASSERT_TRUE(queue.isShutdown(), "Queue should be shutdown");

    return true;
}

bool test_shutdown_unblocks_pop(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);

    std::atomic<bool> pop_completed{false};
    std::thread popper([&queue, &pop_completed]() {
        auto req = queue.pop();
        pop_completed = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_TRUE(!pop_completed, "Pop should be blocked on empty queue");

    queue.shutdown();

    popper.join();
    ASSERT_TRUE(pop_completed, "Shutdown should unblock pop");

    return true;
}

bool test_shutdown_unblocks_push(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(2);

    // Fill the queue
    queue.push(Operation::PUT, "key1", "value1");
    queue.push(Operation::PUT, "key2", "value2");

    std::atomic<bool> push_completed{false};
    std::thread pusher([&queue, &push_completed]() {
        auto future = queue.push(Operation::PUT, "key3", "value3");
        push_completed = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_TRUE(!push_completed, "Push should be blocked when queue is full");

    queue.shutdown();

    pusher.join();
    ASSERT_TRUE(push_completed, "Shutdown should unblock push");

    return true;
}

bool test_push_after_shutdown(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);

    queue.shutdown();

    auto future = queue.push(Operation::PUT, "key1", "value1");
    bool result = future.get();

    ASSERT_TRUE(!result, "Push after shutdown should return false");

    return true;
}

bool test_pop_after_shutdown_returns_empty(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);

    queue.shutdown();

    auto req = queue.pop();
    ASSERT_TRUE(!req.has_value(), "Pop after shutdown should return nullopt");

    return true;
}

bool test_completion_promise(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);

    auto future = queue.push(Operation::PUT, "key1", "value1");

    std::thread processor([&queue]() {
        auto req = queue.pop();
        if (req.has_value()) {
            (*req)->completion.set_value(true);
        }
    });

    bool result = future.get();
    ASSERT_TRUE(result, "Future should return true when promise is set");

    processor.join();

    return true;
}

bool test_multiple_producers_single_consumer(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(1000);
    const int num_producers = 4;
    const int items_per_producer = 100;

    std::vector<std::thread> producers;
    std::atomic<int> total_pushed{0};

    // Start multiple producer threads
    for (int p = 0; p < num_producers; p++) {
        producers.emplace_back([&queue, &total_pushed, p]() {
            for (int i = 0; i < items_per_producer; i++) {
                queue.push(Operation::PUT, "key_" + std::to_string(p) + "_" + std::to_string(i), "value_" + std::to_string(i));
                total_pushed++;
            }
        });
    }

    // Consumer thread
    std::atomic<int> total_popped{0};
    std::thread consumer([&queue, &total_popped]() {
        while (total_popped < num_producers * items_per_producer) {
            auto req = queue.pop();
            if (req.has_value()) {
                total_popped++;
            }
        }
    });

    for (auto &producer : producers) {
        producer.join();
    }

    consumer.join();

    ASSERT_EQ(total_popped.load(), num_producers * items_per_producer, "All items should be consumed");
    ASSERT_EQ(queue.size(), 0, "Queue should be empty");

    return true;
}

bool test_single_producer_multiple_consumers(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(1000);
    const int num_items = 400;
    const int num_consumers = 4;

    std::atomic<int> total_consumed{0};
    std::atomic<bool> producer_done{false};

    // Producer thread
    std::thread producer([&queue, &producer_done]() {
        for (int i = 0; i < num_items; i++) {
            queue.push(Operation::PUT, "key" + std::to_string(i), "value" + std::to_string(i));
        }
        producer_done = true;
    });

    // Multiple consumer threads
    std::vector<std::thread> consumers;

    for (int c = 0; c < num_consumers; c++) {
        consumers.emplace_back([&queue, &total_consumed, &producer_done]() {
            while (true) {
                auto req = queue.pop();
                if (req.has_value()) {
                    total_consumed++;
                    // Exit if we've consumed all items
                    if (total_consumed >= num_items) {
                        break;
                    }
                } else {
                    // Queue was shutdown or empty - check if producer is done
                    if (producer_done && total_consumed >= num_items) {
                        break;
                    }
                    // Small sleep to avoid busy waiting
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        });
    }

    producer.join();

    while (total_consumed < num_items) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    queue.shutdown();

    for (auto &consumer : consumers) {
        consumer.join();
    }

    ASSERT_EQ(total_consumed.load(), num_items, "All items should be consumed");

    return true;
}

bool test_batch_pop_with_multiple_threads(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(1000);
    const int num_items = 1000;

    std::atomic<bool> producer_done{false};

    // Producer
    std::thread producer([&queue, &producer_done]() {
        for (int i = 0; i < num_items; i++) {
            queue.push(Operation::PUT, "key" + std::to_string(i), "value" + std::to_string(i));
        }
        producer_done = true;
    });

    // Consumer using batch pop
    std::atomic<int> total_consumed{0};
    std::thread consumer([&queue, &total_consumed, &producer_done]() {
        while (total_consumed < num_items) {
            auto batch = queue.popBatch(50);
            total_consumed += batch.size();

            // If we got an empty batch and producer is done, we might be finished
            if (batch.empty() && producer_done) {
                break;
            }
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(total_consumed.load(), num_items, "All items should be consumed in batches");

    return true;
}

bool test_empty_key_and_value(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);

    queue.push(Operation::PUT, "", "value");
    queue.push(Operation::PUT, "key", "");
    queue.push(Operation::DELETE, "key2", "");

    auto req1 = queue.pop();
    ASSERT_TRUE(req1.has_value(), "Should handle empty key");
    ASSERT_EQ((*req1)->key, "", "Empty key should be preserved");

    auto req2 = queue.pop();
    ASSERT_TRUE(req2.has_value(), "Should handle empty value");
    ASSERT_EQ((*req2)->value, "", "Empty value should be preserved");

    auto req3 = queue.pop();
    ASSERT_TRUE(req3.has_value(), "Should handle DELETE with empty value");
    ASSERT_EQ((*req3)->op, Operation::DELETE, "DELETE operation should be preserved");

    return true;
}

bool test_special_characters_in_keys(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);

    queue.push(Operation::PUT, "key!@#$%", "value");
    queue.push(Operation::PUT, "key\n\t", "value");
    queue.push(Operation::PUT, "key with spaces", "value");

    auto req1 = queue.pop();
    ASSERT_EQ((*req1)->key, "key!@#$%", "Should handle special characters");

    auto req2 = queue.pop();
    ASSERT_EQ((*req2)->key, "key\n\t", "Should handle whitespace characters");

    auto req3 = queue.pop();
    ASSERT_EQ((*req3)->key, "key with spaces", "Should handle spaces");

    return true;
}

bool test_stress_rapid_push_pop(WriteQueueTest &fixture) {
    fixture.setUp();

    WriteQueue queue(100);
    const int num_operations = 10000;

    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};

    std::thread pusher([&queue, &push_count]() {
        for (int i = 0; i < num_operations; i++) {
            queue.push(Operation::PUT, "key" + std::to_string(i), "value");
            push_count++;
        }
    });

    std::thread popper([&queue, &pop_count]() {
        while (pop_count < num_operations) {
            auto req = queue.pop();
            if (req.has_value()) {
                pop_count++;
            }
        }
    });

    pusher.join();
    popper.join();

    ASSERT_EQ(pop_count.load(), num_operations, "All operations should be processed");

    return true;
}

void run_write_queue_tests(TestFramework &framework) {
    WriteQueueTest fixture;

    std::cout << "Running Write Queue Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    framework.run("test_write_queue_creation", [&]() { return test_write_queue_creation(fixture); });
    framework.run("test_push_and_pop", [&]() { return test_push_and_pop(fixture); });
    framework.run("test_push_multiple_operations", [&]() { return test_push_multiple_operations(fixture); });
    framework.run("test_fifo_order", [&]() { return test_fifo_order(fixture); });
    framework.run("test_pop_batch", [&]() { return test_pop_batch(fixture); });
    framework.run("test_pop_batch_all", [&]() { return test_pop_batch_all(fixture); });
    framework.run("test_pop_batch_empty_queue", [&]() { return test_pop_batch_empty_queue(fixture); });
    framework.run("test_capacity_limit", [&]() { return test_capacity_limit(fixture); });
    framework.run("test_shutdown", [&]() { return test_shutdown(fixture); });
    framework.run("test_shutdown_unblocks_pop", [&]() { return test_shutdown_unblocks_pop(fixture); });
    framework.run("test_shutdown_unblocks_push", [&]() { return test_shutdown_unblocks_push(fixture); });
    framework.run("test_push_after_shutdown", [&]() { return test_push_after_shutdown(fixture); });
    framework.run("test_pop_after_shutdown_returns_empty", [&]() { return test_pop_after_shutdown_returns_empty(fixture); });
    framework.run("test_completion_promise", [&]() { return test_completion_promise(fixture); });
    framework.run("test_multiple_producers_single_consumer", [&]() { return test_multiple_producers_single_consumer(fixture); });
    framework.run("test_single_producer_multiple_consumers", [&]() { return test_single_producer_multiple_consumers(fixture); });
    framework.run("test_batch_pop_with_multiple_threads", [&]() { return test_batch_pop_with_multiple_threads(fixture); });
    framework.run("test_empty_key_and_value", [&]() { return test_empty_key_and_value(fixture); });
    framework.run("test_special_characters_in_keys", [&]() { return test_special_characters_in_keys(fixture); });
    framework.run("test_stress_rapid_push_pop", [&]() { return test_stress_rapid_push_pop(fixture); });
}
