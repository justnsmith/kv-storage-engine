#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

class BloomFilter {
  public:
    explicit BloomFilter(size_t num_elements, double false_positivie_rate = 0.01);

    void add(const std::string &key);

    bool contains(const std::string &key) const;

    std::vector<uint8_t> serialize() const;

    static BloomFilter deserialize(const std::vector<uint8_t> &data);

    size_t size() const;

  private:
    std::vector<bool> bits_;
    size_t num_hash_functions_;
    size_t bit_array_size_;

    std::vector<size_t> hash(const std::string &key) const;
};

#endif
