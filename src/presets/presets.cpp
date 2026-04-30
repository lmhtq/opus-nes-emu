// presets.cpp - INI-based preset scanner / manager.
//
// Preset format (simple INI):
//   name=BluePreset
//   version=1.0
//   game=Super Mario Bros
//   rom_sha256=<sha256 hex>
//   author=lxh
//   description=Crisp visuals
//   type=video|audio|all
//   replacement.<n>.type=...
//   replacement.<n>.original_id=...
//   replacement.<n>.file=...
#include "fcemu/presets.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace fcemu {

PresetManager::PresetManager() = default;

bool PresetManager::init(const std::string& dir) {
    presets_dir_ = dir;
    scan_presets();
    return true;
}

void PresetManager::shutdown() { presets_.clear(); enabled_presets_.clear(); }

static PresetType parse_type(const std::string& s) {
    if (s == "video") return PresetType::Video;
    if (s == "audio") return PresetType::Audio;
    return PresetType::All;
}

bool PresetManager::load_manifest(const std::string& path, PresetManifest& out) {
    std::ifstream f(path);
    if (!f) return false;
    out = PresetManifest{};
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos || line[0] == '#') continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        if (k == "name")              out.name = v;
        else if (k == "version")      out.version = v;
        else if (k == "game")         out.game = v;
        else if (k == "rom_sha256")   out.rom_sha256 = v;
        else if (k == "author")       out.author = v;
        else if (k == "description")  out.description = v;
        else if (k == "type")         out.type = parse_type(v);
        else if (k.rfind("replacement.", 0) == 0) {
            // replacement.<n>.<field>
            auto p1 = k.find('.', 12);
            if (p1 == std::string::npos) continue;
            int n = std::atoi(k.substr(12, p1 - 12).c_str());
            std::string field = k.substr(p1 + 1);
            while ((int)out.replacements.size() <= n) out.replacements.push_back({});
            auto& r = out.replacements[n];
            if (field == "type")             r.type = v;
            else if (field == "original_id") r.original_id = v;
            else if (field == "file")        r.replacement_file = v;
            else                             r.params[field] = v;
        }
    }
    return !out.name.empty();
}

void PresetManager::scan_presets() {
    presets_.clear();
    if (presets_dir_.empty() || !fs::exists(presets_dir_)) return;
    for (auto& e : fs::directory_iterator(presets_dir_)) {
        if (!e.is_regular_file()) continue;
        if (e.path().extension() != ".ini") continue;
        PresetInfo p{};
        p.path = e.path().string();
        p.id = e.path().stem().string();
        if (load_manifest(p.path, p.manifest)) presets_.push_back(std::move(p));
    }
}

PresetInfo* PresetManager::find_matching_preset(const std::string& sha, PresetType type) {
    for (auto& p : presets_) {
        if (p.manifest.rom_sha256 == sha &&
            (p.manifest.type == type || p.manifest.type == PresetType::All) &&
            p.compatible) return &p;
    }
    return nullptr;
}

std::vector<PresetInfo*> PresetManager::find_all_matching(const std::string& sha) {
    std::vector<PresetInfo*> r;
    for (auto& p : presets_) if (p.manifest.rom_sha256 == sha && p.compatible) r.push_back(&p);
    return r;
}

bool PresetManager::enable_preset(const std::string& id) {
    for (auto& p : presets_) {
        if (p.id == id) { p.enabled = true; enabled_presets_[id] = &p; return true; }
    }
    return false;
}

bool PresetManager::disable_preset(const std::string& id) {
    enabled_presets_.erase(id);
    for (auto& p : presets_) if (p.id == id) { p.enabled = false; return true; }
    return false;
}

bool PresetManager::is_enabled(const std::string& id) const { return enabled_presets_.count(id) > 0; }

bool PresetManager::import_preset(const std::string& path) {
    if (!fs::exists(path)) return false;
    fs::create_directories(presets_dir_);
    fs::copy_file(path, fs::path(presets_dir_) / fs::path(path).filename(),
                  fs::copy_options::overwrite_existing);
    scan_presets();
    return true;
}

bool PresetManager::remove_preset(const std::string& id) {
    for (auto& p : presets_) {
        if (p.id == id) { fs::remove(p.path); break; }
    }
    scan_presets();
    return true;
}

std::string PresetManager::get_replacement(const std::string& original_id,
                                           const std::string& type) const {
    for (auto& kv : enabled_presets_) {
        for (auto& r : kv.second->manifest.replacements) {
            if (r.original_id == original_id && r.type == type) return r.replacement_file;
        }
    }
    return {};
}

bool PresetManager::validate_preset(const PresetManifest& m) const {
    return !m.name.empty() && !m.rom_sha256.empty();
}

bool PresetManager::create_template(const std::string& game,
                                    const std::string& sha,
                                    PresetType type) {
    fs::create_directories(presets_dir_);
    std::string id = game; for (char& c : id) if (c == ' ') c = '_';
    std::ofstream f(fs::path(presets_dir_) / (id + ".ini"));
    if (!f) return false;
    f << "name=" << game << "\nversion=1.0\ngame=" << game
      << "\nrom_sha256=" << sha
      << "\nauthor=auto\ndescription=template\n"
      << "type=" << (type == PresetType::Video ? "video" : type == PresetType::Audio ? "audio" : "all")
      << "\n";
    return true;
}

std::string PresetManager::compute_sha256(const std::vector<uint8_t>&) const {
    return {}; // Cartridge owns the canonical hash; not duplicated here.
}

PresetManifest PresetManifest::from_json(const std::string&) { return {}; }
std::string    PresetManifest::to_json() const               { return "{}"; }

} // namespace fcemu
