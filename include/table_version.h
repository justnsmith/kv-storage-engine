#ifndef TABLE_VERSION_H
#define TABLE_VERSION_H

#include "sstable.h"
#include "types.h"
#include <memory>
#include <shared_mutex>
#include <vector>

struct TableVersion {
    std::vector<std::vector<SSTableMeta>> levels;
    std::vector<std::shared_ptr<SSTable>> sstables;
    uint64_t version_number{0};
    uint64_t flush_counter{0};

    TableVersion() = default;

    static std::shared_ptr<TableVersion> copyFrom(const std::shared_ptr<TableVersion> &other);
    std::shared_ptr<SSTable> findSSTableById(uint64_t id) const;
    void addSSTable(std::shared_ptr<SSTable> sst, const SSTableMeta &meta);
    void removeSSTablesByIds(const std::vector<uint64_t> &ids);
};

class VersionManager {
  public:
    VersionManager();

    std::shared_ptr<TableVersion> getCurrentVersion() const;
    void installVersion(std::shared_ptr<TableVersion> newVersion);
    std::shared_ptr<TableVersion> getVersionForModification() const;

  private:
    std::shared_ptr<TableVersion> current_version_;
};

#endif
