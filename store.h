#pragma once
#include "lru.h"
#include "ttl_manager.h"
#include "persistence.h"
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>
#include <optional>

/**
 * Stats — counters exposed by STATS command.
 */
struct Stats {
    uint64_t hits      = 0;
    uint64_t misses    = 0;
    uint64_t evictions = 0;
    uint64_t sets      = 0;
    uint64_t dels      = 0;
    uint64_t expirations = 0;
    size_t   current_keys = 0;
    size_t   capacity     = 0;
};

/**
 * KVStore — the main engine
 *
 * Combines:
 *   - LRUCache          : storage + eviction
 *   - TTLManager        : background expiry
 *   - PersistenceEngine : snapshot save/load
 *
 * Thread safety:
 *   - std::shared_mutex allows concurrent reads (shared lock)
 *   - Writes acquire exclusive (unique) lock
 *   - TTL callback invoked from TTLManager thread locks exclusively
 */
class KVStore {
public:
    static constexpr size_t DEFAULT_CAPACITY = 10'000;
    static constexpr const char* SNAPSHOT_FILE = "snapshot.bin";

    explicit KVStore(size_t capacity = DEFAULT_CAPACITY);
    ~KVStore();

    // SET key value [ttl seconds, -1 = none]
    // Returns name of evicted key or "" if none.
    std::string set(const std::string& key, const std::string& value,
                    long long ttl_seconds = -1);

    // GET key → value or nullopt if missing/expired.
    std::optional<std::string> get(const std::string& key);

    // DEL key → true if key existed.
    bool del(const std::string& key);

    // TTL for key in seconds; -1 = no TTL; 0 = expired.
    long long ttl(const std::string& key) const;

    // List all keys (non-expired).
    std::vector<std::string> keys() const;

    // Flush all keys and TTLs.
    void flush();

    // Persist to disk.
    void save(const std::string& filename = SNAPSHOT_FILE) const;

    // Load from disk; clears existing state.
    void load(const std::string& filename = SNAPSHOT_FILE);

    // Return a copy of stats.
    Stats stats() const;

    size_t size()     const;
    size_t capacity() const;

private:
    // Called by TTLManager when a key expires.
    void onExpire(const std::string& key);

    mutable std::shared_mutex  rw_mutex_;
    LRUCache                   cache_;
    TTLManager                 ttl_mgr_;

    // Atomic counters (no mutex needed for stats).
    mutable std::atomic<uint64_t> hits_{0};
    mutable std::atomic<uint64_t> misses_{0};
    std::atomic<uint64_t>         evictions_{0};
    std::atomic<uint64_t>         sets_{0};
    std::atomic<uint64_t>         dels_{0};
    std::atomic<uint64_t>         expirations_{0};
};
