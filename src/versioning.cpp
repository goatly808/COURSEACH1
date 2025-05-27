#include "versioning.hpp"
#include "crypto.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>

using json = nlohmann::json;
namespace fs = std::filesystem;

VersionManager::VersionManager(const std::string& folder)
    : folder_(folder)
{
    load_manifest();
}

void VersionManager::load_manifest() {
    std::ifstream in(folder_ + "/.manifest.json");
    if (!in) return;
    json j;
    in >> j;
    for (auto& [name, obj] : j.items()) {
        manifest_[name] = {
            obj["version_id"].get<std::string>(),
            obj["hash"].get<std::string>(),
            obj["timestamp"].get<uint64_t>()
        };
    }
}

void VersionManager::save_manifest() {
    json j;
    for (auto& [name, meta] : manifest_) {
        j[name] = {
            {"version_id", meta.version_id},
            {"hash", meta.hash},
            {"timestamp", meta.timestamp}
        };
    }
    std::ofstream out(folder_ + "/.manifest.json");
    out << j.dump(4);
}

std::unordered_map<std::string, FileMeta> VersionManager::scan_folder() {
    std::unordered_map<std::string, FileMeta> current;
    for (auto& p : fs::recursive_directory_iterator(folder_)) {
        if (!p.is_regular_file()) continue;
        std::string full = p.path().string();
        std::string rel  = full.substr(folder_.size() + 1);
        std::ifstream in(full, std::ios::binary);
        std::vector<unsigned char> data(
            (std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>());

        auto hash = hex_encode(sha256(data));
        uint64_t ts = (uint64_t)fs::last_write_time(p).time_since_epoch().count();
        current[rel] = {"", hash, ts};
    }
    return current;
}

void VersionManager::update_manifest() {
    auto current = scan_folder();
    for (auto& [name, meta] : current) {
        if (!manifest_.count(name) || manifest_[name].hash != meta.hash) {
            std::string old_ver = manifest_.count(name) ? manifest_[name].version_id : "0";
            meta.version_id = std::to_string(std::stoll(old_ver) + 1);
            manifest_[name] = meta;
        }
    }
    save_manifest();
}