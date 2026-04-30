// video_enhancer.cpp - Video enhancement (stub)
#include "fcemu/video_enhancer.h"
#include <cstdio>

namespace fcemu {

VideoEnhancer::VideoEnhancer()
    : input_texture_(0), output_texture_(0),
      crt_{}, hdr_{}, aa_mode_(AAMode::None), widescreen_(false),
      passthrough_(false), texture_replacement_enabled_(false),
      shake_intensity_(0.0f), shake_remaining_ms_(0), hit_flash_(false) {}

bool VideoEnhancer::init() {
    printf("VideoEnhancer: Initializing...\n");
    // TODO: Init OpenGL, compile shaders
    return true;
}

void VideoEnhancer::shutdown() {
    // TODO
}

uint32_t VideoEnhancer::process_frame(const uint8_t* rgba_256x240) {
    // TODO: Apply CRT + HDR + AA + replacements
    // For now, just return a placeholder texture ID
    return 0;
}

void VideoEnhancer::set_crt_params(const CRTEffect& params) { crt_ = params; }
void VideoEnhancer::set_hdr_params(const HDRParams& params) { hdr_ = params; }

void VideoEnhancer::set_aa_mode(AAMode mode) { aa_mode_ = mode; }

bool VideoEnhancer::load_texture_replacements(const std::string& preset_path) {
    printf("VideoEnhancer: Loading texture replacements from %s\n", preset_path.c_str());
    // TODO
    return false;
}

void VideoEnhancer::enable_texture_replacement(bool enable) {
    texture_replacement_enabled_ = enable;
}

void VideoEnhancer::enable_widescreen(bool enable) {
    widescreen_ = enable;
}

void VideoEnhancer::trigger_shake(float intensity, int duration_ms) {
    shake_intensity_ = intensity;
    shake_remaining_ms_ = duration_ms;
}

void VideoEnhancer::trigger_hit_flash() {
    hit_flash_ = true;
}

void VideoEnhancer::set_passthrough(bool enable) {
    passthrough_ = enable;
}

uint32_t VideoEnhancer::load_texture(const std::string& path) {
    printf("VideoEnhancer: Loading texture %s\n", path.c_str());
    // TODO: Load PNG, create OpenGL texture
    return 0;
}

} // namespace fcemu
