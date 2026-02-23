#pragma once
#include <list>
#include <string>
#include <unordered_map>
#include <optional>
#include <stdexcept>

/**
 * LRUCache — O(1) Least Recently Used Cache
 *
 * Data Structures:
 *   - std::list<pair<key,value>>  → doubly-linked list (cache ordering)
 *   - std::unordered_map<key, list::iterator> → O(1) lookup
 *
 * Policy:
 *   - GET  : move accessed node to front (most recently used)
 *   - SET  : insert at front; if over capacity, evict from back
 *   - DEL  : erase from both structures in O(1)
 */
class LRUCache {
public:
    using Key   = std::string;
    using Value = std::string;
    using Node  = std::pair<Key, Value>;

    explicit LRUCache(size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) throw std::invalid_argument("LRU capacity must be > 0");
        map_.reserve(capacity_);
    }

    // Returns the value for key, or nullopt if not found.
    // Moves the accessed node to front (marks as recently used).
    std::optional<Value> get(const Key& key) {
        auto it = map_.find(key);
        if (it == map_.end()) return std::nullopt;
        // Splice to front — O(1)
        list_.splice(list_.begin(), list_, it->second);
        return it->second->second;
    }

    // Inserts or updates the key.
    // If key exists, update value and move to front.
    // If capacity exceeded after insert, evict LRU (back of list).
    // Returns the evicted key if one occurred, otherwise "".
    Key set(const Key& key, const Value& value) {
        Key evicted;
        auto it = map_.find(key);
        if (it != map_.end()) {
            // Update in place and move to front
            it->second->second = value;
            list_.splice(list_.begin(), list_, it->second);
        } else {
            // Insert at front
            list_.emplace_front(key, value);
            map_[key] = list_.begin();
            // Evict LRU if over capacity
            if (map_.size() > capacity_) {
                evicted = list_.back().first;
                map_.erase(evicted);
                list_.pop_back();
            }
        }
        return evicted;
    }

    // Removes a key from cache. Returns true if it existed.
    bool del(const Key& key) {
        auto it = map_.find(key);
        if (it == map_.end()) return false;
        list_.erase(it->second);
        map_.erase(it);
        return true;
    }

    // Checks existence without updating recency.
    bool contains(const Key& key) const {
        return map_.count(key) > 0;
    }

    // Expose all entries (in MRU→LRU order) for persistence.
    const std::list<Node>& entries() const { return list_; }

    size_t size()     const { return map_.size(); }
    size_t capacity() const { return capacity_; }

    void clear() {
        list_.clear();
        map_.clear();
    }

private:
    size_t                                        capacity_;
    std::list<Node>                               list_; // front = MRU, back = LRU
    std::unordered_map<Key, std::list<Node>::iterator> map_;
};
