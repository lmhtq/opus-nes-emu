// resource_analyzer.cpp - CHR tile dump + ROM info collection.
#include "fcemu/resource_analyzer.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace fcemu {

// Standard NES palette index 0..3 ↔ greyscale ramp for tiles when no palette known.
static const uint8_t DEFAULT_TILE_PAL[4][3] = {
    {  0,   0,  0},
    { 80,  80,  80},
    {170, 170,170},
    {255, 255,255},
};

ResourceAnalyzer::ResourceAnalyzer() = default;
bool ResourceAnalyzer::init() { return true; }
void ResourceAnalyzer::shutdown() { manifest_ = ResourceManifest{}; }

bool ResourceAnalyzer::analyze_rom(const std::vector<uint8_t>& prg,
                                   const std::vector<uint8_t>& chr,
                                   const RomInfo& info) {
    manifest_ = ResourceManifest{};
    manifest_.rom = info;
    if (!chr.empty()) {
        extract_tiles(chr);
        extract_sprites(chr);
    }
    (void)prg;
    return true;
}

bool ResourceAnalyzer::analyze_rom_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), {});
    if (data.size() < 16 || data[0] != 'N') return false;
    int prg_banks = data[4];
    int chr_banks = data[5];
    size_t prg_off = 16 + ((data[6] & 0x04) ? 512 : 0);
    size_t prg_size = (size_t)prg_banks * 16384;
    size_t chr_off = prg_off + prg_size;
    std::vector<uint8_t> prg(data.begin() + prg_off, data.begin() + prg_off + prg_size);
    std::vector<uint8_t> chr;
    if (chr_banks > 0) {
        chr.assign(data.begin() + chr_off, data.begin() + chr_off + chr_banks * 8192);
    }
    RomInfo info{}; info.name = path; info.prg_rom_size = (int)prg_size / 1024;
    info.chr_rom_size = (int)chr.size() / 1024;
    info.mapper = ((data[6] >> 4) & 0x0F) | (data[7] & 0xF0);
    info.mirroring = (data[6] & 1) ? "vertical" : "horizontal";
    return analyze_rom(prg, chr, info);
}

void ResourceAnalyzer::extract_tiles(const std::vector<uint8_t>& chr) {
    size_t n = chr.size() / 16;
    for (size_t i = 0; i < n; ++i) {
        TileData t{}; t.bank = (uint16_t)(i / 256); t.tile_id = (uint8_t)(i & 0xFF); t.palette = 0;
        decode_tile(chr.data() + i * 16, t.pixels);
        manifest_.tiles.push_back(std::move(t));
    }
}

void ResourceAnalyzer::extract_sprites(const std::vector<uint8_t>&) {}
void ResourceAnalyzer::capture_palette(Ppu*) {}
void ResourceAnalyzer::analyze_audio_patterns(Apu*) {}
void ResourceAnalyzer::on_scanline(int, Ppu*) {}
void ResourceAnalyzer::on_audio_channel(int, Apu*) {}

bool ResourceAnalyzer::export_manifest(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;
    f << "{\n";
    f << "  \"rom\": {\"name\": \"" << manifest_.rom.name << "\"";
    f << ", \"sha256\": \"" << manifest_.rom.sha256 << "\"";
    f << ", \"mapper\": " << manifest_.rom.mapper;
    f << ", \"prg_kb\": " << manifest_.rom.prg_rom_size;
    f << ", \"chr_kb\": " << manifest_.rom.chr_rom_size << "},\n";
    f << "  \"tile_count\": " << manifest_.tiles.size() << "\n";
    f << "}\n";
    return true;
}

void ResourceAnalyzer::decode_tile(const uint8_t* d, std::vector<uint8_t>& rgba,
                                   const uint8_t* /*pal*/) const {
    rgba.assign(8 * 8 * 4, 0);
    for (int y = 0; y < 8; ++y) {
        uint8_t p0 = d[y], p1 = d[y + 8];
        for (int x = 0; x < 8; ++x) {
            int b0 = (p0 >> (7 - x)) & 1;
            int b1 = (p1 >> (7 - x)) & 1;
            int c = b0 | (b1 << 1);
            int idx = (y * 8 + x) * 4;
            rgba[idx + 0] = DEFAULT_TILE_PAL[c][0];
            rgba[idx + 1] = DEFAULT_TILE_PAL[c][1];
            rgba[idx + 2] = DEFAULT_TILE_PAL[c][2];
            rgba[idx + 3] = 255;
        }
    }
}

std::string ResourceManifest::to_json() const {
    std::ostringstream o;
    o << "{\"rom\":\"" << rom.name << "\",\"tiles\":" << tiles.size() << "}";
    return o.str();
}

} // namespace fcemu
