#pragma once
/**
 * ThreadPool.hpp — Fixed-size thread pool implementation
 * 
 * OS CONCEPTS DEMONSTRATED:
 * ─────────────────────────
 * 1. PRODUCER-CONSUMER PATTERN: External callers (producers) enqueue tasks;
 *    worker threads (consumers) dequeue and execute them.
 * 
 * 2. THREAD LIFECYCLE: Worker threads are created at construction and joined
 *    at destruction, demonstrating proper thread lifecycle management.
 * 
 * 3. MUTEX (std::mutex): Protects the shared task queue from concurrent access.
 *    Without this, two workers could dequeue the same task (race condition).
 * 
 * 4. CONDITION VARIABLE (std::condition_variable): Workers sleep when the queue
 *    is empty, avoiding busy-waiting (spin-lock). The condition variable wakes
 *    them when a new task is enqueued or shutdown is requested.
 * 
 * 5. GRACEFUL SHUTDOWN: Setting stop_ = true and calling notify_all() ensures
 *    all workers wake up, finish their current task, drain remaining tasks,
 *    and exit cleanly. This prevents thread leaks and ensures RAII compliance.
 * 
 * 6. RAII: The destructor joins all threads, guaranteeing no detached threads
 *    outlive the ThreadPool object.
 * 
 * WHY A THREAD POOL INSTEAD OF THREAD-PER-REQUEST?
 * ─────────────────────────────────────────────────
 * The original project created a new pthread for every client connection.
 * Problems with thread-per-connection:
 *   - Thread creation is expensive (kernel syscall, stack allocation ~1-8MB)
 *   - No upper bound on thread count → resource exhaustion under load
 *   - Context switching overhead grows linearly with thread count
 * 
 * A thread pool pre-allocates a fixed number of workers. Tasks are queued
 * and distributed, bounding resource usage and amortizing creation cost.
 */

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <stdexcept>
#include <type_traits>

namespace academia {

class ThreadPool {
public:
    /**
     * Construct a thread pool with the given number of worker threads.
     * 
     * Each worker runs an infinite loop:
     *   1. Lock the mutex
     *   2. Wait on the condition variable until (queue non-empty OR stop requested)
     *   3. Dequeue a task
     *   4. Unlock the mutex
     *   5. Execute the task
     *   6. Repeat
     * 
     * The wait-on-condition-variable step is crucial: it atomically releases
     * the mutex and suspends the thread, preventing busy-waiting.
     */
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) {
        if (num_threads == 0) num_threads = 4; // Fallback

        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                worker_loop();
            });
        }
    }

    /**
     * Destructor: graceful shutdown.
     * 
     * 1. Set stop flag (under lock to ensure visibility)
     * 2. Wake ALL waiting workers via notify_all()
     * 3. Join each worker thread (blocks until thread finishes)
     * 
     * This guarantees no thread is left running after the ThreadPool
     * is destroyed — essential for RAII correctness.
     */
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        // Wake all workers so they can see the stop flag
        condition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    // Non-copyable, non-movable (threads reference `this`)
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /**
     * Enqueue a task and return a future for its result.
     * 
     * Uses std::packaged_task to wrap the callable, allowing the caller
     * to retrieve the return value via the associated std::future.
     * 
     * SYNCHRONIZATION:
     *   - Lock queue_mutex_ to safely push onto the shared queue
     *   - notify_one() wakes exactly one waiting worker (efficient:
     *     no thundering-herd problem unlike notify_all)
     */
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using return_type = std::invoke_result_t<F, Args...>;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> result = task->get_future();

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) {
                throw std::runtime_error("enqueue on stopped ThreadPool");
            }
            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        return result;
    }

    /** Returns the number of worker threads in the pool. */
    [[nodiscard]] size_t size() const noexcept { return workers_.size(); }

    /** Returns the current number of pending tasks in the queue. */
    [[nodiscard]] size_t pending() const {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

private:
    /**
     * Worker thread main loop.
     * 
     * CRITICAL SECTION: The queue access (pop) is protected by queue_mutex_.
     * The task execution happens OUTSIDE the lock — this is essential for
     * parallelism. If we held the lock during execution, only one task
     * could run at a time, defeating the purpose of the pool.
     */
    void worker_loop() {
        while (true) {
            std::function<void()> task;
            {
                // RAII lock: automatically released when scope exits
                std::unique_lock<std::mutex> lock(queue_mutex_);

                // Wait until there's a task or shutdown is requested.
                // The predicate prevents SPURIOUS WAKEUPS — the condition
                // variable may wake the thread without notify being called.
                condition_.wait(lock, [this] {
                    return stop_ || !tasks_.empty();
                });

                // If shutting down AND no more tasks, exit the loop
                if (stop_ && tasks_.empty()) {
                    return;
                }

                // Dequeue the next task
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            // Execute task WITHOUT holding the lock
            task();
        }
    }

    std::vector<std::thread> workers_;             // Worker threads
    std::queue<std::function<void()>> tasks_;       // Task queue (FIFO)
    mutable std::mutex queue_mutex_;                // Protects tasks_ and stop_
    std::condition_variable condition_;              // Signals workers
    bool stop_ = false;                              // Shutdown flag
};

} // namespace academia
