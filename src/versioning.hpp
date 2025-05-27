#pragma once
#include <string>
#include <unordered_map>

struct FileMeta {
    std::string version_id;
    std::string hash;
    uint64_t timestamp;
};

class VersionManager {
public:
    explicit VersionManager(const std::string& folder);
    void load_manifest();
    void save_manifest();
    std::unordered_map<std::string, FileMeta> scan_folder();
    void update_manifest();
    std::unordered_map<std::string, FileMeta>& manifest() {
        return manifest_;
     }
    const std::unordered_map<std::string, FileMeta>& manifest() const {
        return manifest_;
    }
    // доступ к манифесту
    const std::unordered_map<std::string, FileMeta>& manifest() const { return manifest_; }

private:
    std::string folder_;
    std::unordered_map<std::string, FileMeta> manifest_;
};
