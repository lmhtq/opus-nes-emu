// resource_analyzer.cpp - Resource analyzer (stub)
#include "fcemu/resource_analyzer.h"
#include <cstdio>
#include <cstring>

namespace fcemu {

ResourceAnalyzer::ResourceAnalyzer() {}

bool ResourceAnalyzer::init() {
    printf("ResourceAnalyzer: Initializing...\n");
    return true;
}

void ResourceAnalyzer::shutdown() {
    manifest_ = ResourceManifest();
}

bool ResourceAnalyzer::analyze_rom(const std::vector<uint8_t>& prg_rom,
                                  const std::vector<uint8_t>& chr_rom,
                                  const RomInfo& info) {
    manifest_.rom = info;
    printf("ResourceAnalyzer: Analyzing ROM '%s' (PRG=%dKB, CHR=%dKB)\n",
           info.name.c_str(), info.prg_rom_size, info.chr_rom_size);
    if (!chr_rom.empty()) {
        extract_tiles(chr_rom);
        extract_sprites(chr_rom);
    }
    return true;
}

bool ResourceAnalyzer::analyze_rom_file(const std::string& rom_path) {
    printf("ResourceAnalyzer: Analyzing ROM file %s\n", rom_path.c_str());
    // TODO: Load and analyze
    return false;
}

const ResourceManifest& ResourceAnalyzer::manifest() const { return manifest_; }

void ResourceAnalyzer::extract_tiles(const std::vector<uint8_t>& chr_rom) {
    size_t num_tiles = chr_rom.size() / 16;  // 16 bytes per 8x8 tile
    printf("ResourceAnalyzer: Extracted %zu tiles\n", num_tiles);
    for (size_t i = 0; i < num_tiles && i < 256; ++i) {
        TileData tile;
        tile.bank = 0;
        tile.tile_id = static_cast<uint8_t>(i);
        tile.palette = 0;
        tile.pixels.resize(8 * 8 * 4);  // RGBA
        decode_tile(chr_rom.data() + i * 16, tile.pixels);
        manifest_.tiles.push_back(tile);
    }
}

void ResourceAnalyzer::extract_sprites(const std::vector<uint8_t>& chr_rom) {
    // TODO: Analyze OAM patterns
}

void ResourceAnalyzer::capture_palette(const Ppu* ppu) {
    // TODO
}

void ResourceAnalyzer::capture_sprites(const Ppu* ppu) {
    // TODO
}

void ResourceAnalyzer::analyze_audio_patterns(const Apu* apu) {
    // TODO
}

bool ResourceAnalyzer::export_manifest(const std::string& path) const {
    printf("ResourceAnalyzer: Export manifest to %s\n", path.c_str());
    // TODO
    return false;
}

void ResourceAnalyzer::on_scanline(int scanline, const Ppu* ppu) {
    // TODO: Runtime capture
}

void ResourceAnalyzer::on_audio_channel(int channel, const Apu* apu) {
    // TODO: Runtime audio analysis
}

void ResourceAnalyzer::decode_tile(const uint8_t* data, std::vector<uint8_t>& rgba,
                                   const uint8_t* palette) const {
    // Simplified: decode 8x8 2bpp tile to RGBA
    rgba.resize(8 * 8 * 4);
    for (int y = 0; y < 8; ++y) {
        uint8_t plane0 = data[y];
        uint8_t plane1 = data[y + 8];
        for (int x = 0; x < 8; ++x) {
            int bit0 = (plane0 >> (7 - x)) & 1;
            int bit1 = (plane1 >> (7 - x)) & 1;
            int color_idx = bit0 | (bit1 << 1);  // 0-3
            int idx = (y * 8 + x) * 4;
            // Use default NES palette for now
            rgba[idx + 0] = color_idx * 85;  // R
            rgba[idx + 1] = color_idx * 85;  // G
            rgba[idx + 2] = color_idx * 85;  // B
            rgba[idx + 3] = 255;          // A
        }
    }
}

void ResourceAnalyzer::decode_sprite(const uint8_t* data, std::vector<uint8_t>& rgba,
                                    uint8_t attr, const uint8_t* palette) const {
    decode_tile(data, rgba, palette);
    // TODO: Handle flip flags from attr
}

}  // namespace fcemu
