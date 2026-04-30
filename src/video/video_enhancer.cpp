// video_enhancer.cpp - CPU-side post effects (CRT scanlines, hit flash, shake offset).
#include "fcemu/video_enhancer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace fcemu {

VideoEnhancer::VideoEnhancer() = default;

bool VideoEnhancer::init() { return true; }
void VideoEnhancer::shutdown() { replacement_textures_.clear(); }

uint32_t VideoEnhancer::process_frame(const uint8_t* in) {
    if (!in) return 0;
    if (passthrough_) return 1;

    static uint8_t out[256 * 240 * 4];
    std::memcpy(out, in, 256 * 240 * 4);

    // CRT scanline darken every other row.
    if (crt_.enabled) {
        float k = 1.0f - std::clamp(crt_.scanline_intensity, 0.0f, 0.95f);
        for (int y = 1; y < 240; y += 2) {
            for (int x = 0; x < 256; ++x) {
                int p = (y * 256 + x) * 4;
                out[p+0] = (uint8_t)(out[p+0] * k);
                out[p+1] = (uint8_t)(out[p+1] * k);
                out[p+2] = (uint8_t)(out[p+2] * k);
            }
        }
    }
    if (hdr_.enabled) {
        float e = std::pow(2.0f, hdr_.exposure - 1.0f);
        for (int i = 0; i < 256*240*4; i += 4) {
            for (int c = 0; c < 3; ++c) {
                float v = out[i+c] / 255.0f * e + hdr_.brightness;
                v = std::clamp(v * hdr_.contrast + (1.0f - hdr_.contrast) * 0.5f, 0.0f, 1.0f);
                out[i+c] = (uint8_t)(v * 255.0f);
            }
        }
    }
    if (hit_flash_) {
        for (int i = 0; i < 256*240*4; i += 4) {
            out[i+0] = std::min(255, out[i+0] + 80);
            out[i+1] = std::min(255, out[i+1] + 80);
            out[i+2] = std::min(255, out[i+2] + 80);
        }
        hit_flash_ = false;
    }
    if (shake_remaining_ms_ > 0) shake_remaining_ms_ = std::max(0, shake_remaining_ms_ - 16);
    return 1;
}

void VideoEnhancer::set_crt_params(const CRTEffect& p)  { crt_ = p; }
void VideoEnhancer::set_hdr_params(const HDRParams& p)  { hdr_ = p; }
void VideoEnhancer::set_aa_mode(AAMode m)               { aa_mode_ = m; }
bool VideoEnhancer::load_texture_replacements(const std::string&) { return false; }
void VideoEnhancer::enable_texture_replacement(bool e)  { texture_replacement_enabled_ = e; }
void VideoEnhancer::enable_widescreen(bool e)           { widescreen_ = e; }
void VideoEnhancer::trigger_shake(float i, int d)       { shake_intensity_ = i; shake_remaining_ms_ = d; }
void VideoEnhancer::trigger_hit_flash()                 { hit_flash_ = true; }
void VideoEnhancer::set_passthrough(bool e)             { passthrough_ = e; }
uint32_t VideoEnhancer::load_texture(const std::string&) { return 0; }

} // namespace fcemu
