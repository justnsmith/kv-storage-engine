#include "wal.h"

WriteAheadLog::WriteAheadLog(const std::string &path, int sync_interval_ms) : path_(path), sync_interval_ms_(sync_interval_ms) {
    std::filesystem::path p(path_);
    std::filesystem::create_directories(p.parent_path());

    fd_ = open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_ == -1) {
        std::cerr << "Failed to open WAL file: " << path_ << " - " << strerror(errno) << std::endl;
    }

    write_buffer_.reserve(MAX_BUFFER_SIZE);
    sync_buffer_.reserve(MAX_BUFFER_SIZE);
    sync_thread_ = std::thread(&WriteAheadLog::syncThreadLoop, this);
}

WriteAheadLog::~WriteAheadLog() {
    shutdown_.store(true);
    {
        std::lock_guard<std::mutex> lock(sync_mutex_);
        sync_requested_ = true;
    }
    sync_cv_.notify_one();

    if (sync_thread_.joinable()) {
        sync_thread_.join();
    }

    if (fd_ != -1) {
        close(fd_);
        fd_ = -1;
    }
}

void WriteAheadLog::syncThreadLoop() {
    while (!shutdown_.load()) {
        std::unique_lock<std::mutex> lock(sync_mutex_);

        if (sync_interval_ms_ > 0) {
            sync_cv_.wait_for(lock, std::chrono::milliseconds(sync_interval_ms_), [this] { return sync_requested_ || shutdown_.load(); });
        } else {
            sync_cv_.wait(lock, [this] { return sync_requested_ || shutdown_.load(); });
        }

        sync_requested_ = false;
        lock.unlock();

        doSync();
    }

    doSync();
}

void WriteAheadLog::doSync() {
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        if (write_buffer_size_ == 0) {
            synced_generation_.store(sync_generation_.load());
            sync_done_cv_.notify_all();
            return;
        }
        std::swap(write_buffer_, sync_buffer_);
        write_buffer_size_ = 0;
        write_buffer_.clear();
    }

    if (!sync_buffer_.empty() && fd_ != -1) {
        ssize_t written = write(fd_, sync_buffer_.data(), sync_buffer_.size());
        if (written == -1) {
            std::cerr << "WAL write failed: " << strerror(errno) << std::endl;
        }

        fsync(fd_);
        sync_buffer_.clear();
    }

    synced_generation_.store(sync_generation_.load());
    sync_done_cv_.notify_all();
}

uint32_t WriteAheadLog::calculateChecksum(Operation op, const std::string &key, const std::string &value, const uint64_t seqNumber) {
    uint32_t crc = crc32(0L, Z_NULL, 0);

    crc = crc32(crc, reinterpret_cast<const Bytef *>(&seqNumber), sizeof(seqNumber));

    uint8_t opByte = static_cast<uint8_t>(op);
    crc = crc32(crc, &opByte, sizeof(opByte));

    uint32_t keyLen = key.size();
    uint32_t valueLen = value.size();
    crc = crc32(crc, reinterpret_cast<const Bytef *>(&keyLen), sizeof(keyLen));
    crc = crc32(crc, reinterpret_cast<const Bytef *>(&valueLen), sizeof(valueLen));

    if (!key.empty()) {
        crc = crc32(crc, reinterpret_cast<const Bytef *>(key.data()), keyLen);
    }
    if (!value.empty()) {
        crc = crc32(crc, reinterpret_cast<const Bytef *>(value.data()), valueLen);
    }

    return crc;
}

void WriteAheadLog::append(Operation op, const std::string &key, const std::string &value, uint64_t seqNumber) {
    uint32_t checksum = calculateChecksum(op, key, value, seqNumber);
    uint32_t keyLen = key.size();
    uint32_t valueLen = value.size();
    uint8_t opByte = static_cast<uint8_t>(op);

    size_t entry_size = sizeof(checksum) + sizeof(seqNumber) + sizeof(opByte) + sizeof(keyLen) + sizeof(valueLen) + keyLen + valueLen;

    std::lock_guard<std::mutex> lock(buffer_mutex_);

    if (write_buffer_size_ + entry_size > MAX_BUFFER_SIZE) {
        {
            std::lock_guard<std::mutex> sync_lock(sync_mutex_);
            sync_requested_ = true;
        }
        sync_cv_.notify_one();
    }

    auto append_to_buffer = [this](const void *data, size_t size) {
        const char *bytes = static_cast<const char *>(data);
        write_buffer_.insert(write_buffer_.end(), bytes, bytes + size);
        write_buffer_size_ += size;
    };

    append_to_buffer(&checksum, sizeof(checksum));
    append_to_buffer(&seqNumber, sizeof(seqNumber));
    append_to_buffer(&opByte, sizeof(opByte));
    append_to_buffer(&keyLen, sizeof(keyLen));
    append_to_buffer(&valueLen, sizeof(valueLen));
    append_to_buffer(key.data(), keyLen);
    append_to_buffer(value.data(), valueLen);
}

void WriteAheadLog::flush() {
    sync_generation_.fetch_add(1);
    uint64_t my_gen = sync_generation_.load();
    {
        std::lock_guard<std::mutex> lock(sync_mutex_);
        sync_requested_ = true;
    }
    sync_cv_.notify_one();

    std::unique_lock<std::mutex> lock(buffer_mutex_);
    sync_done_cv_.wait(lock, [this, my_gen] { return synced_generation_.load() >= my_gen; });
}

// cppcheck-suppress unusedFunction
void WriteAheadLog::syncFlush() {
    sync_generation_.fetch_add(1);
    uint64_t my_gen = sync_generation_.load();

    {
        std::lock_guard<std::mutex> lock(sync_mutex_);
        sync_requested_ = true;
    }
    sync_cv_.notify_one();

    std::unique_lock<std::mutex> lock(buffer_mutex_);
    sync_done_cv_.wait(lock, [this, my_gen] { return synced_generation_.load() >= my_gen; });
}

void WriteAheadLog::replay(std::function<void(uint64_t, Operation, std::string &, std::string &)> apply) {
    std::ifstream inputFile(path_, std::ios::in | std::ios::binary);
    if (!inputFile) {
        return;
    }
    uint32_t checksum{};
    uint64_t seqNumber{};
    uint8_t opByte{};
    uint32_t keyLen{};
    uint32_t valueLen{};
    while (inputFile.read(reinterpret_cast<char *>(&checksum), sizeof(checksum))) {
        inputFile.read(reinterpret_cast<char *>(&seqNumber), sizeof(seqNumber));
        inputFile.read(reinterpret_cast<char *>(&opByte), sizeof(opByte));
        inputFile.read(reinterpret_cast<char *>(&keyLen), sizeof(keyLen));
        inputFile.read(reinterpret_cast<char *>(&valueLen), sizeof(valueLen));
        Operation op = static_cast<Operation>(opByte);
        std::string key(keyLen, '\0');
        std::string value(valueLen, '\0');
        inputFile.read(&key[0], keyLen);
        inputFile.read(&value[0], valueLen);
        uint32_t newChecksum = calculateChecksum(op, key, value, seqNumber);
        if (newChecksum == checksum) {
            apply(seqNumber, op, key, value);
        } else {
            std::cerr << "Data has been corrupted\n";
            break;
        }
    }
}

bool WriteAheadLog::empty() const {
    if (!std::filesystem::exists(path_)) {
        return true;
    }
    return std::filesystem::file_size(path_) == 0;
}
