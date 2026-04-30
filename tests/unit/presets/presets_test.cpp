// presets_test.cpp - INI manifest parsing.
#include "fcemu/presets.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main() {
    printf("=== fcemu Presets Tests ===\n");
    auto dir = fs::temp_directory_path() / "fcemu_presets_test";
    fs::create_directories(dir);
    {
        std::ofstream f(dir / "demo.ini");
        f << "name=Demo\nversion=1.0\ngame=Smb\nrom_sha256=abc123\n"
             "author=lxh\ndescription=test\ntype=video\n"
             "replacement.0.type=tile\nreplacement.0.original_id=42\n"
             "replacement.0.file=tile42.png\n";
    }
    fcemu::PresetManager pm;
    pm.init(dir.string());
    assert(pm.presets().size() == 1);
    auto* p = pm.find_matching_preset("abc123", fcemu::PresetType::Video);
    assert(p);
    assert(p->manifest.name == "Demo");
    assert(p->manifest.replacements.size() == 1);
    assert(p->manifest.replacements[0].replacement_file == "tile42.png");
    pm.enable_preset(p->id);
    assert(pm.get_replacement("42", "tile") == "tile42.png");
    fs::remove_all(dir);
    printf("PASS\n");
    return 0;
}
