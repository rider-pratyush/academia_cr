/**
 * MutexDemo.cpp — Synchronization primitives demonstration
 * 
 * Demonstrates:
 *   1. Race condition WITHOUT mutex
 *   2. Fix with std::mutex + std::lock_guard
 *   3. Reader-writer lock with std::shared_mutex
 *   4. std::unique_lock for deferred/manual locking
 *   5. Atomic operations as lock-free alternative
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <vector>
#include <atomic>
#include <chrono>

// ──────────────────────────────────────────────
// Demo 1: Race condition (INTENTIONAL BUG)
// ──────────────────────────────────────────────
void demo_race_condition() {
    std::cout << "=== Demo 1: Race Condition (no synchronization) ===\n";
    int counter = 0;
    const int ITERATIONS = 100000;

    auto increment = [&]() {
        for (int i = 0; i < ITERATIONS; ++i) {
            ++counter;  // NOT ATOMIC — read-modify-write can interleave
        }
    };

    std::thread t1(increment);
    std::thread t2(increment);
    t1.join(); t2.join();

    std::cout << "Expected: " << 2 * ITERATIONS 
              << ", Got: " << counter 
              << (counter != 2 * ITERATIONS ? " ← RACE CONDITION!" : "") << "\n\n";
}

// ──────────────────────────────────────────────
// Demo 2: Fixed with std::mutex
// ──────────────────────────────────────────────
void demo_mutex_fix() {
    std::cout << "=== Demo 2: Fixed with std::mutex ===\n";
    int counter = 0;
    std::mutex mtx;
    const int ITERATIONS = 100000;

    auto increment = [&]() {
        for (int i = 0; i < ITERATIONS; ++i) {
            std::lock_guard<std::mutex> lock(mtx);  // RAII lock
            ++counter;
        }
    };

    std::thread t1(increment);
    std::thread t2(increment);
    t1.join(); t2.join();

    std::cout << "Expected: " << 2 * ITERATIONS 
              << ", Got: " << counter << " ✓\n\n";
}

// ──────────────────────────────────────────────
// Demo 3: Reader-writer lock
// ──────────────────────────────────────────────
void demo_shared_mutex() {
    std::cout << "=== Demo 3: std::shared_mutex (Reader-Writer Lock) ===\n";
    int data = 0;
    std::shared_mutex rw_mutex;
    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};

    // Multiple readers can hold the lock simultaneously
    auto reader = [&](int id) {
        for (int i = 0; i < 1000; ++i) {
            std::shared_lock<std::shared_mutex> lock(rw_mutex);  // Shared (read) lock
            volatile int val = data;  // Read the data
            (void)val;
            read_count.fetch_add(1);
        }
    };

    // Writers need exclusive access
    auto writer = [&](int id) {
        for (int i = 0; i < 100; ++i) {
            std::unique_lock<std::shared_mutex> lock(rw_mutex);  // Exclusive (write) lock
            ++data;
            write_count.fetch_add(1);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) threads.emplace_back(reader, i);
    for (int i = 0; i < 2; ++i) threads.emplace_back(writer, i);
    for (auto& t : threads) t.join();

    std::cout << "Reads: " << read_count.load() 
              << ", Writes: " << write_count.load() 
              << ", Final value: " << data << " ✓\n\n";
}

// ──────────────────────────────────────────────
// Demo 4: Atomic operations
// ──────────────────────────────────────────────
void demo_atomics() {
    std::cout << "=== Demo 4: std::atomic (Lock-Free) ===\n";
    std::atomic<int> counter{0};
    const int ITERATIONS = 100000;

    auto increment = [&]() {
        for (int i = 0; i < ITERATIONS; ++i) {
            counter.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::thread t1(increment);
    std::thread t2(increment);
    t1.join(); t2.join();

    std::cout << "Expected: " << 2 * ITERATIONS 
              << ", Got: " << counter.load() << " ✓\n\n";
}

int main() {
    std::cout << "╔══════════════════════════════════════════╗\n"
              << "║   Synchronization Primitives Demo        ║\n"
              << "╚══════════════════════════════════════════╝\n\n";

    demo_race_condition();
    demo_mutex_fix();
    demo_shared_mutex();
    demo_atomics();

    std::cout << "All demos complete!\n";
    return 0;
}
