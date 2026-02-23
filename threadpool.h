#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

/**
 * ThreadPool — fixed-size pool for concurrent command execution
 *
 * Design:
 *   - N worker threads block on a shared job queue.
 *   - Callers push std::function<void()> jobs; workers pop and execute.
 *   - Graceful shutdown: sets stop_ flag then notifies all workers.
 *
 * Usage:
 *   ThreadPool pool(4);
 *   pool.enqueue([] { process_command(cmd); });
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = 4) : stop_(false) {
        if (num_threads == 0) throw std::invalid_argument("Thread pool size must be > 0");
        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
    }

    // Submit a job. Thread-safe.
    template<typename F>
    void enqueue(F&& job) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stop_) throw std::runtime_error("ThreadPool already stopped");
            jobs_.emplace(std::forward<F>(job));
        }
        cv_.notify_one();
    }

    size_t numThreads() const { return workers_.size(); }

private:
    void workerLoop() {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
                if (stop_ && jobs_.empty()) return;
                job = std::move(jobs_.front());
                jobs_.pop();
            }
            job(); // Execute outside lock
        }
    }

    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> jobs_;
    std::mutex                        mutex_;
    std::condition_variable           cv_;
    bool                              stop_;
};
