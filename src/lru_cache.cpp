#include "lru_cache.h"

LRUCache::LRUCache(size_t capacity) : capacity_(capacity) {
}

std::optional<Entry> LRUCache::get(const std::string &key) {
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return std::nullopt;
    }

    lru_list_.splice(lru_list_.begin(), lru_list_, it->second.list_iter);

    return it->second.entry;
}

void LRUCache::put(const std::string &key, const Entry &entry) {
    auto it = cache_.find(key);

    if (it != cache_.end()) {
        it->second.entry = entry;
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second.list_iter);
        return;
    }

    if (cache_.size() >= capacity_) {
        const std::string &lru_key = lru_list_.back();
        cache_.erase(lru_key);
        lru_list_.pop_back();
    }

    lru_list_.push_front(key);
    cache_[key] = CacheNode{entry, lru_list_.begin()};
}

void LRUCache::clear() {
    cache_.clear();
    lru_list_.clear();
}

void LRUCache::invalidate(const std::string &key) {
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        lru_list_.erase(it->second.list_iter);
        cache_.erase(it);
    }
}

size_t LRUCache::size() const {
    return cache_.size();
}
