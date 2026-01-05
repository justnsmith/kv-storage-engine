#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include "types.h"
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

class LRUCache {
  public:
    explicit LRUCache(size_t capacity = 1000);
    std::optional<Entry> get(const std::string &key);
    void put(const std::string &key, const Entry &entry);
    void clear();
    void invalidate(const std::string &key);
    size_t size() const;

  private:
    struct CacheNode {
        Entry entry;
        std::list<std::string>::iterator list_iter;
    };

    mutable std::mutex mutex_;
    size_t capacity_;
    std::list<std::string> lru_list_;
    std::unordered_map<std::string, CacheNode> cache_;
};

#endif
