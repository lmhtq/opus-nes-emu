// video_enhancer.cpp - CPU-side post effects (CRT, HDR, hit flash, shake) +
// widescreen edge-extension to 320x240.
#include "fcemu/video_enhancer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace fcemu {

VideoEnhancer::VideoEnhancer() = default;

bool VideoEnhancer::init() {
    out_buf_.assign(256 * 240 * 4, 0);
    out_w_ = 256; out_h_ = 240;
    return true;
}
void VideoEnhancer::shutdown() { replacement_textures_.clear(); out_buf_.clear(); }

namespace {
inline void apply_postfx(uint8_t* px, int w, int h, const CRTEffect& crt, const HDRParams& hdr,
                         bool& hit_flash) {
    if (crt.enabled) {
        float k = 1.0f - std::clamp(crt.scanline_intensity, 0.0f, 0.95f);
        for (int y = 1; y < h; y += 2) {
            for (int x = 0; x < w; ++x) {
                int p = (y * w + x) * 4;
                px[p+0] = (uint8_t)(px[p+0] * k);
                px[p+1] = (uint8_t)(px[p+1] * k);
                px[p+2] = (uint8_t)(px[p+2] * k);
            }
        }
    }
    if (hdr.enabled) {
        float e = std::pow(2.0f, hdr.exposure - 1.0f);
        for (int i = 0; i < w * h * 4; i += 4) {
            for (int c = 0; c < 3; ++c) {
                float v = px[i+c] / 255.0f * e + hdr.brightness;
                v = std::clamp(v * hdr.contrast + (1.0f - hdr.contrast) * 0.5f, 0.0f, 1.0f);
                px[i+c] = (uint8_t)(v * 255.0f);
            }
        }
    }
    if (hit_flash) {
        for (int i = 0; i < w * h * 4; i += 4) {
            px[i+0] = std::min(255, px[i+0] + 80);
            px[i+1] = std::min(255, px[i+1] + 80);
            px[i+2] = std::min(255, px[i+2] + 80);
        }
        hit_flash = false;
    }
}
} // namespace

const uint8_t* VideoEnhancer::process(const uint8_t* in, int* out_w, int* out_h) {
    if (!in) { if (out_w) *out_w = 0; if (out_h) *out_h = 0; return nullptr; }
    const int W = widescreen_ ? 320 : 256;
    const int H = 240;
    if ((int)out_buf_.size() != W * H * 4) out_buf_.assign(W * H * 4, 0);
    out_w_ = W; out_h_ = H;

    if (passthrough_ && !widescreen_) {
        std::memcpy(out_buf_.data(), in, 256 * 240 * 4);
    } else if (!widescreen_) {
        std::memcpy(out_buf_.data(), in, 256 * 240 * 4);
    } else {
        // Widescreen 320x240 = 32 left + 256 center + 32 right.
        // Side bands: edge column replicated and dimmed (vignette).
        for (int y = 0; y < H; ++y) {
            const uint8_t* src_row = in + y * 256 * 4;
            uint8_t* dst_row = out_buf_.data() + y * W * 4;
            // Center copy.
            std::memcpy(dst_row + 32 * 4, src_row, 256 * 4);
            // Left band: replicate column 0, ramp brightness 0.4 -> 1.0.
            for (int x = 0; x < 32; ++x) {
                float k = 0.4f + 0.6f * (x / 32.0f);
                dst_row[x*4+0] = (uint8_t)(src_row[0] * k);
                dst_row[x*4+1] = (uint8_t)(src_row[1] * k);
                dst_row[x*4+2] = (uint8_t)(src_row[2] * k);
                dst_row[x*4+3] = 255;
            }
            // Right band: replicate column 255, ramp 1.0 -> 0.4.
            const uint8_t* edge = src_row + 255 * 4;
            for (int x = 0; x < 32; ++x) {
                float k = 1.0f - 0.6f * (x / 32.0f);
                int dx = (288 + x) * 4;
                dst_row[dx+0] = (uint8_t)(edge[0] * k);
                dst_row[dx+1] = (uint8_t)(edge[1] * k);
                dst_row[dx+2] = (uint8_t)(edge[2] * k);
                dst_row[dx+3] = 255;
            }
        }
    }

    if (!passthrough_) {
        apply_postfx(out_buf_.data(), W, H, crt_, hdr_, hit_flash_);
    }
    if (shake_remaining_ms_ > 0) shake_remaining_ms_ = std::max(0, shake_remaining_ms_ - 16);
    if (out_w) *out_w = W;
    if (out_h) *out_h = H;
    return out_buf_.data();
}

uint32_t VideoEnhancer::process_frame(const uint8_t* in) {
    int w = 0, h = 0;
    return process(in, &w, &h) ? 1 : 0;
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
