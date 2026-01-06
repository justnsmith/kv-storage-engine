#include "bloom_filter.h"
#include <functional>

BloomFilter::BloomFilter(size_t num_elements, double false_positivie_rate) {
    bit_array_size_ = static_cast<size_t>(-1.0 * num_elements * std::log(false_positivie_rate) / (std::log(2) * std::log(2)));

    num_hash_functions_ = static_cast<size_t>((static_cast<double>(bit_array_size_) / num_elements) * std::log(2));

    if (num_hash_functions_ < 1)
        num_hash_functions_ = 1;

    bits_.resize(bit_array_size_, false);
}

std::vector<size_t> BloomFilter::hash(const std::string &key) const {
    std::vector<size_t> hashes;
    hashes.reserve(num_hash_functions_);

    std::hash<std::string> hasher;
    size_t hash1 = hasher(key);
    size_t hash2 = hasher(key + "salt");

    for (size_t i = 0; i < num_hash_functions_; i++) {
        size_t combined_hash = hash1 + i * hash2;
        hashes.push_back(combined_hash % bit_array_size_);
    }

    return hashes;
}

size_t BloomFilter::size() const {
    return bits_.size();
}

void BloomFilter::add(const std::string &key) {
    auto hashes = hash(key);
    for (size_t h : hashes) {
        bits_[h] = true;
    }
}

bool BloomFilter::contains(const std::string &key) const {
    auto hashes = hash(key);
    return std::all_of(hashes.begin(), hashes.end(), [this](size_t h) { return bits_[h]; });
}

std::vector<uint8_t> BloomFilter::serialize() const {
    std::vector<uint8_t> data;

    data.resize(sizeof(size_t) * 3);

    size_t offset = 0;
    std::memcpy(data.data() + offset, &bit_array_size_, sizeof(size_t));
    offset += sizeof(size_t);

    std::memcpy(data.data() + offset, &num_hash_functions_, sizeof(size_t));
    offset += sizeof(size_t);

    size_t num_bytes = (bit_array_size_ + 7) / 8;
    std::memcpy(data.data() + offset, &num_bytes, sizeof(size_t));
    offset += sizeof(size_t);

    data.resize(offset + num_bytes);
    for (size_t i = 0; i < bit_array_size_; i++) {
        if (bits_[i]) {
            size_t byte_index = i / 8;
            size_t bit_index = i % 8;
            data[offset + byte_index] |= (1 << bit_index);
        }
    }
    return data;
}

BloomFilter BloomFilter::deserialize(const std::vector<uint8_t> &data) {
    size_t offset = 0;

    size_t bit_array_size;
    std::memcpy(&bit_array_size, data.data() + offset, sizeof(size_t));
    offset += sizeof(size_t);

    size_t num_hash_functions;
    std::memcpy(&num_hash_functions, data.data() + offset, sizeof(size_t));
    offset += sizeof(size_t);

    size_t num_bytes;
    std::memcpy(&num_bytes, data.data() + offset, sizeof(size_t));
    offset += sizeof(size_t);

    BloomFilter filter(1, 0.01);
    filter.bit_array_size_ = bit_array_size;
    filter.num_hash_functions_ = num_hash_functions;
    filter.bits_.resize(bit_array_size, false);

    for (size_t i = 0; i < bit_array_size; i++) {
        size_t byte_index = i / 8;
        size_t bit_index = i % 8;
        if (data[offset + byte_index] & (1 << bit_index)) {
            filter.bits_[i] = true;
        }
    }
    return filter;
}
