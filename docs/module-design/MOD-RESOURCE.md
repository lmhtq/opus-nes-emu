# MOD-RESOURCE: 游戏资源分析器#

## 元数据 (Metadata)

- **ID**: MOD-RESOURCE
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-115
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 功能职责 (Responsibilities)

实现游戏资源分析器，自动提取可替换的资源。

核心职责：
1. ROM 自动分析（识别游戏、读取 CHR/PRG）
2. 音乐提取（识别 APU 各通道旋律）
3. 音效提取（识别常见音效）
4. Tile 提取（从 CHR ROM/RAM）
5. 精灵提取（从 OAM 和图案表）
6. 调色板提取
7. 资源清单生成（JSON manifest）
8. 资源预览（在 UI 中显示）

## 接口设计 (Interface Design)

```cpp
// include/fcemu/resource_analyzer.h
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <json/json.h>

namespace fcemu {

// ROM 信息
struct RomInfo {
    std::string name;
    std::string sha256;
    int prg_rom_size;   // KB
    int chr_rom_size;    // KB
    int mapper;
    std::string mirroring;
};

// Tile 数据
struct TileData {
    uint16_t bank;          // CHR bank
    uint16_t tile_id;       // Tile 编号（0-255）
    uint8_t palette;        // 使用的调色板（0-3）
    std::vector<uint8_t> pixels;  // 8x8, 2bpp, RGBA
};

// 精灵数据
struct SpriteData {
    uint8_t oam_index;      // OAM 索引（0-63）
    uint8_t tile_id;
    uint8_t attributes;       // 翻转变换、优先级等
    int x, y;                // 屏幕位置
    std::vector<uint8_t> pixels;  // RGBA 数据
};

// 音乐轨道
struct MusicTrack {
    int channel;            // APU 通道（0-4）
    std::string name;         // 如 "overworld", "battle"
    std::vector<uint8_t> pattern;  // 音符模式
};

// 音效
struct SoundEffect {
    std::string name;       // 如 "jump", "explosion", "shoot"
    int trigger_address;       // 触发的内存地址（可选）
    int channel;              // APU 通道
    std::vector<uint8_t> sample_data;
};

// 调色板
struct PaletteData {
    uint16_t addr;          // PPU 地址（$3F00-$3F1F）
    std::vector<uint8_t> colors;  // 4 种颜色（RGB）
};

// 资源清单
struct ResourceManifest {
    RomInfo rom;
    std::vector<TileData> tiles;
    std::vector<SpriteData> sprites;
    std::vector<MusicTrack> music;
    std::vector<SoundEffect> sfx;
    std::vector<PaletteData> palettes;
    std::string to_json() const;  // 输出 JSON manifest
};

// 资源分析器
class ResourceAnalyzer {
public:
    ResourceAnalyzer();

    bool init();
    void shutdown();

    // 分析 ROM
    bool analyze_rom(const std::vector<uint8_t>& prg_rom,
                     const std::vector<uint8_t>& chr_rom,
                     const RomInfo& info);
    bool analyze_rom_file(const std::string& rom_path);

    // 获取分析结果
    const ResourceManifest& manifest() const { return manifest_; }
    const RomInfo& rom_info() const { return manifest_.rom; }

    // Tile 操作
    const std::vector<TileData>& tiles() const { return manifest_.tiles; }
    TileData get_tile(uint16_t bank, uint16_t tile_id) const;
    bool export_tile_png(const TileData& tile, const std::string& path) const;

    // 精灵操作
    const std::vector<SpriteData>& sprites() const { return manifest_.sprites; }
    bool export_sprite_png(const SpriteData& sprite, const std::string& path) const;

    // 音乐轨道
    const std::vector<MusicTrack>& music_tracks() const { return manifest_.music; }
    bool export_music_track(const MusicTrack& track, const std::string& path) const;

    // 调色板
    const std::vector<PaletteData>& palettes() const { return manifest_.palettes; }

    // 导出清单
    bool export_manifest(const std::string& path) const;

    // 实时分析（运行时）
    void on_scanline(int scanline, const class Ppu* ppu);
    void on_audio_channel(int channel, const class Apu* apu);

private:
    ResourceManifest manifest_;

    // CHR ROM 解析
    void extract_tiles(const std::vector<uint8_t>& chr_rom);
    void extract_sprites(const std::vector<uint8_t>& chr_rom);

    // PPU 运行时数据
    void capture_palette(const Ppu* ppu);
    void capture_sprites(const Ppu* ppu);

    // APU 运行时数据
    void analyze_audio_patterns(const Apu* apu);

    // 辅助
    void decode_tile(const uint8_t* data, std::vector<uint8_t>& rgba,
                    const uint8_t* palette = nullptr) const;
    void decode_sprite(const uint8_t* data, std::vector<uint8_t>& rgba,
                      uint8_t attr, const uint8_t* palette) const;
};

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-CPU | 分析 PRG ROM 指令模式 |
| MOD-PPU | 提取 Tile/精灵/调色板 |
| MOD-APU | 提取音乐/音效 |
| MOD-CARTRIDGE | 读取 CHR/PRG ROM |

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/cpu/instruction-set.md` - CPU 指令分析
- `docs/hardware/ppu/rendering.md` - Tile/精灵提取
- `docs/hardware/apu/audio-channels.md` - 音频分析

## 变更记录 (Change History)

- 2026-04-30: Initial version
