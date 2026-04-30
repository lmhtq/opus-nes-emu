// include/fcemu/presets.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace fcemu {

enum class PresetType { Video, Audio, All };

struct Replacement {
    std::string type;
    std::string original_id;
    std::string replacement_file;
    std::map<std::string, std::string> params;
};

struct PresetManifest {
    std::string name;
    std::string version;
    std::string game;
    std::string rom_sha256;
    std::string author;
    std::string description;
    PresetType type;
    std::vector<Replacement> replacements;
    static PresetManifest from_json(const std::string& json_str);
    std::string to_json() const;
};

struct PresetInfo {
    std::string id;
    PresetManifest manifest;
    std::string path;
    bool enabled = false;
    bool compatible = true;
};

class PresetManager {
public:
    PresetManager();
    bool init(const std::string& presets_dir);
    void shutdown();
    void scan_presets();
    const std::vector<PresetInfo>& presets() const { return presets_; }
    PresetInfo* find_matching_preset(const std::string& rom_sha256, PresetType type);
    std::vector<PresetInfo*> find_all_matching(const std::string& rom_sha256);
    bool enable_preset(const std::string& preset_id);
    bool disable_preset(const std::string& preset_id);
    bool is_enabled(const std::string& preset_id) const;
    bool import_preset(const std::string& path);
    bool remove_preset(const std::string& preset_id);
    std::string get_replacement(const std::string& original_id,
                               const std::string& type) const;
    bool validate_preset(const PresetManifest& manifest) const;
    bool create_template(const std::string& game_name,
                         const std::string& rom_sha256,
                         PresetType type);

private:
    std::string presets_dir_;
    std::vector<PresetInfo> presets_;
    std::map<std::string, PresetInfo*> enabled_presets_;
    std::string compute_sha256(const std::vector<uint8_t>& data) const;
    bool load_manifest(const std::string& preset_path, PresetManifest& out);
};

} // namespace fcemu
