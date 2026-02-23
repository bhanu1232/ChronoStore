#include "store.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

KVStore::KVStore(size_t capacity)
    : cache_(capacity),
      ttl_mgr_(std::chrono::milliseconds(500))
{
    // Wire the TTL expiry callback
    ttl_mgr_.setExpireCallback([this](const std::string& key) {
        onExpire(key);
    });
    ttl_mgr_.start();
}

KVStore::~KVStore() {
    ttl_mgr_.stop();
}

// ─────────────────────────────────────────────────────────────────────────────
// SET
// ─────────────────────────────────────────────────────────────────────────────

std::string KVStore::set(const std::string& key, const std::string& value,
                          long long ttl_seconds)
{
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    std::string evicted = cache_.set(key, value);
    if (!evicted.empty()) {
        ttl_mgr_.remove(evicted);
        ++evictions_;
    }

    // Register TTL if specified
    if (ttl_seconds > 0) {
        ttl_mgr_.set(key, std::chrono::seconds(ttl_seconds));
    } else {
        // Clear any previous TTL on this key (e.g., re-SET without EX)
        ttl_mgr_.remove(key);
    }

    ++sets_;
    return evicted;
}

// ─────────────────────────────────────────────────────────────────────────────
// GET
// ─────────────────────────────────────────────────────────────────────────────

std::optional<std::string> KVStore::get(const std::string& key)
{
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    auto result = cache_.get(key);
    if (result) {
        ++hits_;
    } else {
        ++misses_;
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// DEL
// ─────────────────────────────────────────────────────────────────────────────

bool KVStore::del(const std::string& key)
{
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    bool existed = cache_.del(key);
    if (existed) {
        ttl_mgr_.remove(key);
        ++dels_;
    }
    return existed;
}

// ─────────────────────────────────────────────────────────────────────────────
// TTL
// ─────────────────────────────────────────────────────────────────────────────

long long KVStore::ttl(const std::string& key) const
{
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    if (!cache_.contains(key)) return -2; // key doesn't exist
    return ttl_mgr_.ttl(key);
}

// ─────────────────────────────────────────────────────────────────────────────
// KEYS
// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::string> KVStore::keys() const
{
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    std::vector<std::string> result;
    for (auto& [k, v] : cache_.entries()) {
        result.push_back(k);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// FLUSH
// ─────────────────────────────────────────────────────────────────────────────

void KVStore::flush()
{
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    cache_.clear();
    // TTLManager doesn't have a bulk-clear public API but entries expire harmlessly
    // We rely on TTLManager::snapshot being empty after stop/restart or harmless misfires.
    // Simple approach: no keys → onExpire is a no-op.
}

// ─────────────────────────────────────────────────────────────────────────────
// SAVE
// ─────────────────────────────────────────────────────────────────────────────

void KVStore::save(const std::string& filename) const
{
    std::vector<SnapshotEntry> entries;
    {
        std::shared_lock<std::shared_mutex> lock(rw_mutex_);
        for (auto& [k, v] : cache_.entries()) {
            long long remaining_ms = ttl_mgr_.ttl_ms(k);
            if (remaining_ms == 0) continue; // already expired, skip
            SnapshotEntry e;
            e.key    = k;
            e.value  = v;
            e.ttl_ms = remaining_ms; // -1 if no TTL
            entries.push_back(std::move(e));
        }
    }
    PersistenceEngine::save(filename, entries);
}

// ─────────────────────────────────────────────────────────────────────────────
// LOAD
// ─────────────────────────────────────────────────────────────────────────────

void KVStore::load(const std::string& filename)
{
    auto raw = PersistenceEngine::load(filename);
    auto now = Clock::now();

    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    cache_.clear();
    for (auto& e : raw) {
        if (e.ttl_ms == 0) continue; // expired during load

        cache_.set(e.key, e.value);

        if (e.ttl_ms > 0) {
            // Reconstruct absolute deadline
            auto deadline = now + std::chrono::milliseconds(e.ttl_ms);
            ttl_mgr_.setAbsolute(e.key, deadline);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// STATS
// ─────────────────────────────────────────────────────────────────────────────

Stats KVStore::stats() const
{
    Stats s;
    s.hits         = hits_.load();
    s.misses       = misses_.load();
    s.evictions    = evictions_.load();
    s.sets         = sets_.load();
    s.dels         = dels_.load();
    s.expirations  = expirations_.load();
    s.current_keys = size();
    s.capacity     = capacity();
    return s;
}

size_t KVStore::size() const
{
    std::shared_lock<std::shared_mutex> lock(rw_mutex_);
    return cache_.size();
}

size_t KVStore::capacity() const
{
    return cache_.capacity();
}

// ─────────────────────────────────────────────────────────────────────────────
// Private: TTL expiry callback (called from TTLManager worker thread)
// ─────────────────────────────────────────────────────────────────────────────

void KVStore::onExpire(const std::string& key)
{
    std::unique_lock<std::shared_mutex> lock(rw_mutex_);
    if (cache_.del(key)) {
        ++expirations_;
    }
}
