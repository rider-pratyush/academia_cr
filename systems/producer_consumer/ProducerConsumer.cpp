/**
 * ProducerConsumer.cpp — C++20 Producer-Consumer pattern demo
 * 
 * OS CONCEPTS:
 *   - PRODUCER-CONSUMER PROBLEM: Multiple producers add items to a bounded buffer,
 *     multiple consumers remove items. The buffer must be protected from:
 *       1. Race conditions (concurrent access)
 *       2. Overflow (producer adding to a full buffer)
 *       3. Underflow (consumer reading from an empty buffer)
 *   
 *   - MUTEX: Protects the shared buffer from concurrent modification
 *   - CONDITION VARIABLES:
 *       not_full: producers wait when buffer is full
 *       not_empty: consumers wait when buffer is empty
 *   - SPURIOUS WAKEUPS: The wait predicate prevents acting on false signals
 * 
 * This is a direct C++20 rewrite of the original master-worker.c from the project.
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <atomic>
#include <chrono>

class BoundedBuffer {
public:
    explicit BoundedBuffer(size_t capacity) : capacity_(capacity) {}

    void produce(int item, int producer_id) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait until buffer has space. The predicate prevents spurious wakeups.
        not_full_.wait(lock, [this] { return buffer_.size() < capacity_; });
        
        buffer_.push(item);
        std::cout << "Produced " << item << " by producer " << producer_id 
                  << " [buffer: " << buffer_.size() << "/" << capacity_ << "]\n";
        
        // Notify consumers that an item is available
        not_empty_.notify_one();
    }

    int consume(int consumer_id) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        // Wait until buffer has items
        not_empty_.wait(lock, [this] { return !buffer_.empty() || done_; });
        
        if (buffer_.empty() && done_) return -1;  // No more items
        
        int item = buffer_.front();
        buffer_.pop();
        std::cout << "Consumed " << item << " by consumer " << consumer_id 
                  << " [buffer: " << buffer_.size() << "/" << capacity_ << "]\n";
        
        // Notify producers that space is available
        not_full_.notify_one();
        return item;
    }

    void mark_done() {
        std::lock_guard<std::mutex> lock(mutex_);
        done_ = true;
        not_empty_.notify_all();  // Wake all waiting consumers
    }

private:
    std::queue<int> buffer_;
    size_t capacity_;
    std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    bool done_ = false;
};

int main(int argc, char* argv[]) {
    int total_items = 20;
    int buffer_size = 5;
    int num_producers = 3;
    int num_consumers = 4;

    if (argc >= 5) {
        total_items = std::atoi(argv[1]);
        buffer_size = std::atoi(argv[2]);
        num_consumers = std::atoi(argv[3]);
        num_producers = std::atoi(argv[4]);
    }

    std::cout << "=== Producer-Consumer Demo ===\n"
              << "Items: " << total_items << ", Buffer: " << buffer_size
              << ", Producers: " << num_producers << ", Consumers: " << num_consumers << "\n\n";

    BoundedBuffer buffer(buffer_size);
    std::atomic<int> next_item{0};
    std::atomic<int> consumed_count{0};

    // Producer threads
    std::vector<std::thread> producers;
    for (int i = 0; i < num_producers; ++i) {
        producers.emplace_back([&, i] {
            while (true) {
                int item = next_item.fetch_add(1);
                if (item >= total_items) break;
                buffer.produce(item, i);
            }
        });
    }

    // Consumer threads
    std::vector<std::thread> consumers;
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([&, i] {
            while (true) {
                int item = buffer.consume(i);
                if (item == -1) break;
                consumed_count.fetch_add(1);
            }
        });
    }

    // Wait for all producers to finish
    for (auto& t : producers) t.join();
    
    // Signal that production is complete
    buffer.mark_done();
    
    // Wait for all consumers to finish
    for (auto& t : consumers) t.join();

    std::cout << "\n=== Complete ===\n"
              << "Total consumed: " << consumed_count.load() << "/" << total_items << "\n";

    return 0;
}
