#include "sstable.h"

SSTable::SSTable(const std::string &path) : path_(path) {
    loadMetadata();
}

SSTable SSTable::flush(const std::map<std::string, Entry> &snapshot, const std::string &dir_path, uint64_t flush_counter) {
    std::string full_path = dir_path + "sstable_" + std::to_string(flush_counter) + ".bin";

    try {
        if (!std::filesystem::exists(dir_path)) {
            std::filesystem::create_directories(dir_path);
        }
    } catch (const std::filesystem::filesystem_error &e) {
        throw std::runtime_error("Failed to create directory: " + dir_path + " Error: " + e.what());
    }

    SSTable table(full_path);

    std::ofstream sstableFile(full_path, std::ios::out | std::ios::binary);
    if (!sstableFile) {
        throw std::runtime_error("Failed to open SSTable file: " + full_path);
    }

    table.bloom_filter_ = std::make_unique<BloomFilter>(snapshot.size(), BLOOM_FP_RATE);

    size_t entry_count = 0;

    for (const auto &[k, v] : snapshot) {

        table.bloom_filter_->add(k);

        if (entry_count % INDEX_INTERVAL == 0) {
            uint64_t offset = sstableFile.tellp();
            table.index_.push_back(IndexEntry{k, offset});
        }
        entry_count++;

        uint32_t keyLen = k.size();
        uint32_t valueLen = v.value.size();
        const char *keyBytes = k.data();
        const char *valueBytes = v.value.data();

        sstableFile.write(reinterpret_cast<const char *>(&v.seq), sizeof(v.seq));
        sstableFile.write(reinterpret_cast<const char *>(&v.type), sizeof(v.type));
        sstableFile.write(reinterpret_cast<const char *>(&keyLen), sizeof(keyLen));
        sstableFile.write(reinterpret_cast<const char *>(&valueLen), sizeof(valueLen));
        sstableFile.write(keyBytes, keyLen);
        sstableFile.write(valueBytes, valueLen);
    }

    table.min_key_ = snapshot.begin()->first;
    table.max_key_ = snapshot.rbegin()->first;

    uint32_t minKeyLen = table.min_key_.size();
    uint32_t maxKeyLen = table.max_key_.size();

    table.metadata_offset_ = sstableFile.tellp();

    // Write metadata
    sstableFile.write(reinterpret_cast<const char *>(&minKeyLen), sizeof(minKeyLen));
    sstableFile.write(reinterpret_cast<const char *>(&maxKeyLen), sizeof(maxKeyLen));
    sstableFile.write(table.min_key_.data(), minKeyLen);
    sstableFile.write(table.max_key_.data(), maxKeyLen);

    // Write the index to file
    uint32_t indexSize = table.index_.size();
    sstableFile.write(reinterpret_cast<const char *>(&indexSize), sizeof(indexSize));

    for (const auto &entry : table.index_) {
        uint32_t keyLen = entry.key.size();
        sstableFile.write(reinterpret_cast<const char *>(&keyLen), sizeof(keyLen));
        sstableFile.write(entry.key.data(), keyLen);
        sstableFile.write(reinterpret_cast<const char *>(&entry.offset), sizeof(entry.offset));
    }

    // Write bloom filter
    std::vector<uint8_t> bloom_data = table.bloom_filter_->serialize();
    uint32_t bloomSize = bloom_data.size();
    sstableFile.write(reinterpret_cast<const char *>(&bloomSize), sizeof(bloomSize));
    sstableFile.write(reinterpret_cast<const char *>(bloom_data.data()), bloomSize);

    sstableFile.write(reinterpret_cast<const char *>(&table.metadata_offset_), sizeof(table.metadata_offset_));

    return table;
}

std::optional<Entry> SSTable::get(const std::string &key) const {
    if (key < min_key_ || key > max_key_) {
        return std::nullopt;
    }

    if (bloom_filter_ && !bloom_filter_->contains(key)) {
        return std::nullopt;
    }

    std::ifstream file(path_, std::ios::binary);
    if (!file)
        throw std::runtime_error("Failed to open SSTable: " + path_);

    // Binary search index to find start position
    uint64_t search_start = 0;
    uint64_t search_end = metadata_offset_;

    if (!index_.empty()) {
        auto it = std::upper_bound(index_.begin(), index_.end(), key,
                                   [](const std::string &k, const IndexEntry &entry) { return k < entry.key; });
        if (it != index_.begin()) {
            --it;
            search_start = it->offset;
        }

        if (it != index_.end() && (it + 1) != index_.end()) {
            search_end = (it + 1)->offset;
        }
    }

    file.seekg(search_start);

    while (file.tellg() < static_cast<std::streampos>(search_end)) {
        uint64_t seq;
        EntryType type;
        uint32_t keyLen, valueLen;

        if (!file.read(reinterpret_cast<char *>(&seq), sizeof(seq)))
            break;
        file.read(reinterpret_cast<char *>(&type), sizeof(type));
        file.read(reinterpret_cast<char *>(&keyLen), sizeof(keyLen));
        file.read(reinterpret_cast<char *>(&valueLen), sizeof(valueLen));

        std::string k(keyLen, '\0');
        std::string v(valueLen, '\0');

        file.read(k.data(), keyLen);
        if (type == EntryType::PUT) {
            file.read(v.data(), valueLen);
        }

        if (k == key) {
            return Entry{v, seq, type};
        }

        if (k > key) {
            break;
        }
    }
    return std::nullopt;
}

std::map<std::string, Entry> SSTable::getData() const {
    std::map<std::string, Entry> data;
    std::ifstream file(path_, std::ios::binary);

    while (file.tellg() < static_cast<std::streampos>(metadata_offset_)) {
        uint64_t seq;
        EntryType type;
        uint32_t keyLen, valueLen;

        if (!file.read(reinterpret_cast<char *>(&seq), sizeof(seq)))
            break;
        file.read(reinterpret_cast<char *>(&type), sizeof(type));
        file.read(reinterpret_cast<char *>(&keyLen), sizeof(keyLen));
        file.read(reinterpret_cast<char *>(&valueLen), sizeof(valueLen));

        std::string k(keyLen, '\0');
        std::string v(valueLen, '\0');

        file.read(k.data(), keyLen);
        if (type == EntryType::PUT) {
            file.read(v.data(), valueLen);
        }

        data[k] = Entry{v, seq, type};
    }
    return data;
}

void SSTable::loadMetadata() {
    std::ifstream sstableFile(path_, std::ios::binary);
    if (!sstableFile)
        return;

    sstableFile.seekg(-static_cast<int>(sizeof(uint64_t)), std::ios::end);
    sstableFile.read(reinterpret_cast<char *>(&metadata_offset_), sizeof(metadata_offset_));

    sstableFile.seekg(metadata_offset_);

    uint32_t minKeyLen, maxKeyLen;
    sstableFile.read(reinterpret_cast<char *>(&minKeyLen), sizeof(minKeyLen));
    sstableFile.read(reinterpret_cast<char *>(&maxKeyLen), sizeof(maxKeyLen));

    min_key_.resize(minKeyLen);
    max_key_.resize(maxKeyLen);

    sstableFile.read(&min_key_[0], minKeyLen);
    sstableFile.read(&max_key_[0], maxKeyLen);

    uint32_t indexSize;
    sstableFile.read(reinterpret_cast<char *>(&indexSize), sizeof(indexSize));

    index_.reserve(indexSize);
    for (uint32_t i = 0; i < indexSize; i++) {
        uint32_t keyLen;
        sstableFile.read(reinterpret_cast<char *>(&keyLen), sizeof(keyLen));

        std::string key(keyLen, '\0');
        sstableFile.read(&key[0], keyLen);

        uint64_t offset;
        sstableFile.read(reinterpret_cast<char *>(&offset), sizeof(offset));

        index_.push_back(IndexEntry{key, offset});
    }

    uint32_t bloomSize;
    sstableFile.read(reinterpret_cast<char *>(&bloomSize), sizeof(bloomSize));

    std::vector<uint8_t> bloom_data(bloomSize);
    sstableFile.read(reinterpret_cast<char *>(bloom_data.data()), bloomSize);

    bloom_filter_ = std::make_unique<BloomFilter>(BloomFilter::deserialize(bloom_data));
}

const std::string &SSTable::filename() const {
    return path_;
}

SSTable::Iterator::Iterator(const SSTable &table) : file_(table.path_, std::ios::binary), data_end_(table.metadata_offset_) {
    if (!file_) {
        throw std::runtime_error("Failed to open the SSTable: " + table.path_);
    }

    file_.seekg(0);

    readNext();
}

void SSTable::Iterator::readNext() {
    if (file_.tellg() >= static_cast<std::streampos>(data_end_)) {
        valid_ = false;
        return;
    }

    uint64_t seq;
    EntryType type;
    uint32_t keyLen, valueLen;

    if (!file_.read(reinterpret_cast<char *>(&seq), sizeof(seq))) {
        valid_ = false;
        return;
    }

    file_.read(reinterpret_cast<char *>(&type), sizeof(type));
    file_.read(reinterpret_cast<char *>(&keyLen), sizeof(keyLen));
    file_.read(reinterpret_cast<char *>(&valueLen), sizeof(valueLen));

    std::string key(keyLen, '\0');
    std::string value(valueLen, '\0');

    file_.read(key.data(), keyLen);
    if (type == EntryType::PUT) {
        file_.read(value.data(), valueLen);
    }

    current_ = SSTableEntry{key, value, seq, type};
    valid_ = true;
}

void SSTable::Iterator::next() {
    readNext();
}

bool SSTable::Iterator::valid() const {
    return valid_;
}

const SSTableEntry &SSTable::Iterator::entry() const {
    return current_;
}
