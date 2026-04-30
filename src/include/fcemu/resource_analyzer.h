// include/fcemu/resource_analyzer.h
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>

namespace fcemu {

struct RomInfo {
    std::string name;
    std::string sha256;
    int prg_rom_size;
    int chr_rom_size;
    int mapper;
    std::string mirroring;
};

struct TileData {
    uint16_t bank;
    uint16_t tile_id;
    uint8_t palette;
    std::vector<uint8_t> pixels;
};

struct SpriteData {
    uint8_t oam_index;
    uint8_t tile_id;
    uint8_t attributes;
    int x, y;
    std::vector<uint8_t> pixels;
};

struct MusicTrack {
    int channel;
    std::string name;
    std::vector<uint8_t> pattern;
};

struct SoundEffect {
    std::string name;
    int trigger_address;
    int channel;
    std::vector<uint8_t> sample_data;
};

struct PaletteData {
    uint16_t addr;
    std::vector<uint8_t> colors;
};

struct ResourceManifest {
    RomInfo rom;
    std::vector<TileData> tiles;
    std::vector<SpriteData> sprites;
    std::vector<MusicTrack> music;
    std::vector<SoundEffect> sfx;
    std::vector<PaletteData> palettes;
    std::string to_json() const;
};

class ResourceAnalyzer {
public:
    ResourceAnalyzer();
    bool init();
    void shutdown();
    bool analyze_rom(const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom,
                     const RomInfo& info);
    bool analyze_rom_file(const std::string& rom_path);
    const ResourceManifest& manifest() const { return manifest_; }
    const RomInfo& rom_info() const { return manifest_.rom; }
    const std::vector<TileData>& tiles() const { return manifest_.tiles; }
    const std::vector<SpriteData>& sprites() const { return manifest_.sprites; }
    const std::vector<MusicTrack>& music_tracks() const { return manifest_.music; }
    const std::vector<PaletteData>& palettes() const { return manifest_.palettes; }
    bool export_manifest(const std::string& path) const;
    void on_scanline(int scanline, class Ppu* ppu);
    void on_audio_channel(int channel, class Apu* apu);

private:
    ResourceManifest manifest_;
    void extract_tiles(const std::vector<uint8_t>& chr_rom);
    void extract_sprites(const std::vector<uint8_t>& chr_rom);
    void capture_palette(class Ppu* ppu);
    void analyze_audio_patterns(class Apu* apu);
    void decode_tile(const uint8_t* data, std::vector<uint8_t>& rgba,
                    const uint8_t* palette = nullptr) const;
};

} // namespace fcemu
