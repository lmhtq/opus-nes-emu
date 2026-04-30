# MOD-VIDEO: 画质增强

## 元数据 (Metadata)

- **ID**: MOD-VIDEO
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-101, REQ-102, REQ-103, REQ-104, REQ-105
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 功能职责 (Responsibilities)

实现画质增强功能，让 FC 游戏视觉更具冲击力。

核心职责：
1. CRT 扫描线效果（可调强度）
2. CRT 曲率效果（屏幕弯曲）
3. CRT 光晕效果（像素发光扩散）
4. CRT 色散效果（RGB 通道偏移）
5. HDR 色调映射
6. 抗锯齿（边缘平滑）
7. 亮度/对比度/饱和度调节
8. 高清纹理替换（Tile 级）
9. 宽屏补丁（16:9）
10. 视觉特效增强（爆炸震动、动态模糊、粒子）
11. 预制视觉包管理
12. 原始/增强实时切换

## 接口设计 (Interface Design)

```cpp
// include/fcemu/video.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace fcemu {

// CRT 效果参数
struct CRTEffect {
    float scanline_intensity = 0.5f;   // 0-1
    float curvature = 0.3f;            // 0-1
    float bloom_radius = 2.0f;          // pixels
    float bloom_intensity = 0.3f;
    float chromatic_shift = 1.0f;       // pixels
    bool enabled = true;
};

// HDR/Tone mapping
struct HDRParams {
    float exposure = 1.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    float brightness = 0.0f;
    bool enabled = false;
};

// 纹理替换
struct TextureReplacement {
    uint16_t bank;       // CHR bank
    uint16_t tile_id;     // Tile 编号
    uint8_t palette_id;   // 调色板（可选）
    std::string file_path; // PNG 文件路径
};

// 视觉增强器
class VideoEnhancer {
public:
    VideoEnhancer();

    // 初始化（OpenGL 上下文已创建）
    bool init();
    void shutdown();

    // 处理帧（输入原始 256x240 RGBA，输出处理后的纹理）
    uint32_t process_frame(const uint8_t* rgba_256x240);

    // CRT 效果
    void set_crt_params(const CRTEffect& params);
    const CRTEffect& crt_params() const { return crt_; }

    // HDR/Tone mapping
    void set_hdr_params(const HDRParams& params);
    const HDRParams& hdr_params() const { return hdr_; }

    // 抗锯齿
    enum class AAMode { None, FXAA, SMAA };
    void set_aa_mode(AAMode mode);

    // 纹理替换
    bool load_texture_replacements(const std::string& preset_path);
    void enable_texture_replacement(bool enable);

    // 宽屏
    void enable_widescreen(bool enable);
    bool widescreen_enabled() const { return widescreen_; }

    // 视觉特效
    void trigger_screen_shake(float intensity, int duration_ms);
    void trigger_hit_flash();  // 受伤闪红

    // 原始/增强切换
    void set_passthrough(bool enable);  // true = 原始画面

private:
    // OpenGL
    uint32_t input_texture_;
    uint32_t output_texture_;
    uint32_t framebuffer_;

    // Shaders
    uint32_t crt_shader_;
    uint32_t aa_shader_;
    uint32_t hdr_shader_;
    uint32_t bloom_shader_;

    // 状态
    CRTEffect crt_;
    HDRParams hdr_;
    AAMode aa_mode_ = AAMode::None;
    bool widescreen_ = false;
    bool passthrough_ = false;

    // 纹理替换
    std::map<uint32_t, uint32_t> replacement_textures_;  // key = hash(bank, tile, palette)
    bool texture_replacement_enabled_ = false;

    // 特效状态
    float shake_intensity_ = 0.0f;
    int shake_remaining_ms_ = 0;
    bool hit_flash_ = false;

    // 辅助
    uint32_t load_texture(const std::string& path);
    void compile_shaders();
};

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-PPU | 获取原始渲染输出（256x240） |
| MOD-UI | 显示增强后的画面 |
| MOD-RESOURCE | 获取可替换的纹理资源 |
| MOD-PRESETS | 加载预制视觉包 |

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/ppu/rendering.md` - PPU 渲染参考
- `docs/hardware/ppu/sprites.md` - 精灵参考

## 变更记录 (Change History)

- 2026-04-30: Initial version
