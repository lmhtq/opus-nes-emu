// presets.cpp - Preset manager (stub)
#include "fcemu/presets.h"
#include <cstdio>
#include <cstring>

namespace fcemu {

PresetManager::PresetManager() {}

bool PresetManager::init(const std::string& presets_dir) {
    presets_dir_ = presets_dir;
    printf("PresetManager: Init with directory %s\n", presets_dir.c_str());
    scan_presets();
    return true;
}

void PresetManager::shutdown() {
    presets_.clear();
    enabled_presets_.clear();
}

void PresetManager::scan_presets() {
    printf("PresetManager: Scanning for presets in %s\n", presets_dir_.c_str());
    // TODO: Scan directory for manifest.json files
}

PresetInfo* PresetManager::find_matching_preset(const std::string& rom_sha256, PresetType type) {
    for (auto& p : presets_) {
        if (p.manifest.rom_sha256 == rom_sha256 && p.manifest.type == type && p.compatible) {
            return &p;
        }
    }
    return nullptr;
}

std::vector<PresetInfo*> PresetManager::find_all_matching(const std::string& rom_sha256) {
    std::vector<PresetInfo*> result;
    for (auto& p : presets_) {
        if (p.manifest.rom_sha256 == rom_sha256 && p.compatible) {
            result.push_back(&p);
        }
    }
    return result;
}

bool PresetManager::enable_preset(const std::string& preset_id) {
    printf("PresetManager: Enable preset %s\n", preset_id.c_str());
    for (auto& p : presets_) {
        if (p.id == preset_id) {
            p.enabled = true;
            enabled_presets_[preset_id] = &p;
            return true;
        }
    }
    return false;
}

bool PresetManager::disable_preset(const std::string& preset_id) {
    printf("PresetManager: Disable preset %s\n", preset_id.c_str());
    enabled_presets_.erase(preset_id);
    for (auto& p : presets_) {
        if (p.id == preset_id) { p.enabled = false; return true; }
    }
    return false;
}

bool PresetManager::is_enabled(const std::string& preset_id) const {
    return enabled_presets_.count(preset_id) > 0;
}

bool PresetManager::import_preset(const std::string& path) {
    printf("PresetManager: Import preset from %s\n", path.c_str());
    // TODO
    return false;
}

bool PresetManager::remove_preset(const std::string& preset_id) {
    printf("PresetManager: Remove preset %s\n", preset_id.c_str());
    // TODO
    return false;
}

std::string PresetManager::get_replacement(const std::string& original_id,
                                            const std::string& type) const {
    // TODO: Lookup in enabled presets
    return "";
}

bool PresetManager::validate_preset(const PresetManifest& manifest) const {
    // TODO
    return true;
}

bool PresetManager::create_template(const std::string& game_name,
                                   const std::string& rom_sha256,
                                   PresetType type) {
    printf("PresetManager: Create template for %s\n", game_name.c_str());
    // TODO
    return false;
}

bool PresetManager::load_manifest(const std::string& preset_path, PresetManifest& out) {
    // TODO: Parse manifest.json
    return false;
}

std::string PresetManager::compute_sha256(const std::vector<uint8_t>& data) const {
    // TODO: Compute SHA-256
    return "placeholder_sha256";
}

}  // namespace fcemu
