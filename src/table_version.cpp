#include "table_version.h"

#include <algorithm>

std::shared_ptr<TableVersion> TableVersion::copyFrom(const std::shared_ptr<TableVersion> &other) {
    auto newVersion = std::make_shared<TableVersion>();
    if (other) {
        newVersion->levels = other->levels;
        newVersion->sstables = other->sstables;
        newVersion->version_number = other->version_number + 1;
        newVersion->flush_counter = other->flush_counter;
    }
    return newVersion;
}

std::shared_ptr<SSTable> TableVersion::findSSTableById(uint64_t id) const {
    std::string idPattern = "_" + std::to_string(id) + ".";
    auto it = std::find_if(sstables.begin(), sstables.end(), [&idPattern](const std::shared_ptr<SSTable> &sst) {
        return sst && sst->filename().find(idPattern) != std::string::npos;
    });
    return (it != sstables.end()) ? *it : nullptr;
}

void TableVersion::addSSTable(std::shared_ptr<SSTable> sst, const SSTableMeta &meta) {
    sstables.push_back(std::move(sst));

    if (meta.level >= levels.size()) {
        levels.resize(meta.level + 1);
    }
    levels[meta.level].push_back(meta);
}

void TableVersion::removeSSTablesByIds(const std::vector<uint64_t> &ids) {
    sstables.erase(std::remove_if(sstables.begin(), sstables.end(),
                                  [&ids](const std::shared_ptr<SSTable> &sst) {
                                      if (!sst)
                                          return true;
                                      return std::any_of(ids.begin(), ids.end(), [&sst](uint64_t id) {
                                          return sst->filename().find("_" + std::to_string(id) + ".") != std::string::npos;
                                      });
                                  }),
                   sstables.end());

    // Remove from level metadata
    for (auto &level : levels) {
        level.erase(std::remove_if(level.begin(), level.end(),
                                   [&ids](const SSTableMeta &meta) { return std::find(ids.begin(), ids.end(), meta.id) != ids.end(); }),
                    level.end());
    }
}

VersionManager::VersionManager() {
    std::atomic_store(&current_version_, std::make_shared<TableVersion>());
}

std::shared_ptr<TableVersion> VersionManager::getCurrentVersion() const {
    return std::atomic_load(&current_version_);
}

void VersionManager::installVersion(std::shared_ptr<TableVersion> newVersion) {
    std::atomic_store(&current_version_, std::move(newVersion));
}

std::shared_ptr<TableVersion> VersionManager::getVersionForModification() const {
    return TableVersion::copyFrom(std::atomic_load(&current_version_));
}
