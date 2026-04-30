// include/fcemu/video_enhancer.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace fcemu {

struct CRTEffect {
    float scanline_intensity = 0.5f;
    float curvature = 0.3f;
    float bloom_radius = 2.0f;
    float bloom_intensity = 0.3f;
    float chromatic_shift = 1.0f;
    bool enabled = true;
};

struct HDRParams {
    float exposure = 1.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;
    float brightness = 0.0f;
    bool enabled = false;
};

enum class AAMode { None, FXAA, SMAA };

struct TextureReplacement {
    uint16_t bank;
    uint16_t tile_id;
    uint8_t palette_id;
    std::string file_path;
};

class VideoEnhancer {
public:
    VideoEnhancer();
    bool init();
    void shutdown();
    uint32_t process_frame(const uint8_t* rgba_256x240);
    void set_crt_params(const CRTEffect& params);
    void set_hdr_params(const HDRParams& params);
    void set_aa_mode(AAMode mode);
    bool load_texture_replacements(const std::string& preset_path);
    void enable_texture_replacement(bool enable);
    void enable_widescreen(bool enable);
    void trigger_shake(float intensity, int duration_ms);
    void trigger_hit_flash();
    void set_passthrough(bool enable);

private:
    uint32_t input_texture_;
    uint32_t output_texture_;
    CRTEffect crt_;
    HDRParams hdr_;
    AAMode aa_mode_ = AAMode::None;
    bool widescreen_ = false;
    bool passthrough_ = false;
    std::map<uint32_t, uint32_t> replacement_textures_;
    bool texture_replacement_enabled_ = false;
    float shake_intensity_ = 0.0f;
    int shake_remaining_ms_ = 0;
    bool hit_flash_ = false;
    uint32_t load_texture(const std::string& path);
};

} // namespace fcemu
