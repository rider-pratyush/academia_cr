/**
 * ThreadPoolDemo.cpp — Standalone thread pool demonstration
 * 
 * This is a self-contained demo of the same ThreadPool used in the main application.
 * Run it to see tasks being distributed across worker threads.
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <future>
#include <vector>
#include <chrono>

class DemoThreadPool {
public:
    explicit DemoThreadPool(size_t n) {
        std::cout << "Creating thread pool with " << n << " workers\n";
        for (size_t i = 0; i < n; ++i) {
            workers_.emplace_back([this, i] {
                std::cout << "  Worker " << i << " started (thread " << std::this_thread::get_id() << ")\n";
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    ~DemoThreadPool() {
        { std::lock_guard<std::mutex> lock(mutex_); stop_ = true; }
        cv_.notify_all();
        for (auto& w : workers_) w.join();
    }

    template<typename F>
    auto submit(F&& f) -> std::future<decltype(f())> {
        auto task = std::make_shared<std::packaged_task<decltype(f())()>>(std::forward<F>(f));
        auto future = task->get_future();
        { std::lock_guard<std::mutex> lock(mutex_); tasks_.push([task] { (*task)(); }); }
        cv_.notify_one();
        return future;
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};

int main() {
    std::cout << "╔══════════════════════════════════════════╗\n"
              << "║          Thread Pool Demo                ║\n"
              << "╚══════════════════════════════════════════╝\n\n";

    {
        DemoThreadPool pool(4);
        std::vector<std::future<int>> results;

        std::cout << "\nSubmitting 20 tasks...\n\n";
        for (int i = 0; i < 20; ++i) {
            results.push_back(pool.submit([i] {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                std::cout << "  Task " << i << " executed by thread " << std::this_thread::get_id() << "\n";
                return i * i;
            }));
        }

        std::cout << "\nResults: ";
        for (auto& f : results) std::cout << f.get() << " ";
        std::cout << "\n";
    }

    std::cout << "\nPool destroyed — all workers joined\n";
    return 0;
}
