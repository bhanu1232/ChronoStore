#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

using Clock     = std::chrono::steady_clock;
using TimePoint = std::chrono::steady_clock::time_point;

/**
 * TTLManager — background expiry engine
 *
 * Maintains a map of key → expiry_time.
 * A dedicated std::thread wakes every `interval_ms` milliseconds,
 * scans for expired keys, and calls the user-supplied `on_expire` callback.
 *
 * Thread safety: all map accesses are protected by a std::mutex.
 * Shutdown: destructor signals the thread via condition_variable.
 */
class TTLManager {
public:
    using ExpireCallback = std::function<void(const std::string&)>;

    explicit TTLManager(std::chrono::milliseconds interval = std::chrono::milliseconds(500))
        : interval_(interval), running_(false) {}

    ~TTLManager() { stop(); }

    // Register a callback that is invoked with the expired key.
    void setExpireCallback(ExpireCallback cb) { on_expire_ = std::move(cb); }

    // Set or refresh TTL for a key (absolute deadline).
    void set(const std::string& key, std::chrono::seconds ttl_secs) {
        std::lock_guard<std::mutex> lock(mutex_);
        expiry_map_[key] = Clock::now() + ttl_secs;
    }

    // Remove TTL entry (e.g. when key is DEL'd manually).
    void remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        expiry_map_.erase(key);
    }

    // Returns remaining TTL in seconds; -1 if no TTL; -2 if already expired.
    long long ttl(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = expiry_map_.find(key);
        if (it == expiry_map_.end()) return -1;
        auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
            it->second - Clock::now()
        ).count();
        return remaining > 0 ? remaining : 0;
    }

    // Returns remaining TTL in milliseconds for persistence use.
    long long ttl_ms(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = expiry_map_.find(key);
        if (it == expiry_map_.end()) return -1;
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            it->second - Clock::now()
        ).count();
        return remaining > 0 ? remaining : 0;
    }

    // Re-insert with absolute deadline (used during snapshot load).
    void setAbsolute(const std::string& key, TimePoint deadline) {
        std::lock_guard<std::mutex> lock(mutex_);
        expiry_map_[key] = deadline;
    }

    void start() {
        running_ = true;
        worker_  = std::thread(&TTLManager::run, this);
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }

    bool isRunning() const { return running_.load(); }

    // Snapshot helper: get a snapshot of the TTL map.
    std::unordered_map<std::string, TimePoint> snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return expiry_map_;
    }

private:
    void run() {
        while (true) {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, interval_, [this] { return !running_.load(); });
            if (!running_) break;

            // Collect expired keys first to avoid modifying map during iteration
            std::vector<std::string> expired_keys;
            auto now = Clock::now();
            for (auto& [key, deadline] : expiry_map_) {
                if (now >= deadline) expired_keys.push_back(key);
            }
            for (auto& key : expired_keys) {
                expiry_map_.erase(key);
            }
            lock.unlock(); // Release before calling callback (callback acquires store mutex)

            if (on_expire_) {
                for (auto& key : expired_keys) {
                    on_expire_(key);
                }
            }
        }
    }

    std::chrono::milliseconds                      interval_;
    std::atomic<bool>                              running_;
    mutable std::mutex                             mutex_;
    std::condition_variable                        cv_;
    std::thread                                    worker_;
    std::unordered_map<std::string, TimePoint>     expiry_map_;
    ExpireCallback                                 on_expire_;
};
