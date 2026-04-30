// haptics.cpp - SDL2-based haptics + dominant-color RGB lighting hook.
#include "fcemu/haptics.h"
#include <SDL.h>
#include <algorithm>
#include <cstdio>

namespace fcemu {

HapticsManager::HapticsManager() : current_color_{255,255,255}, current_light_mode_(LightMode::Static) {}
HapticsManager::~HapticsManager() { shutdown(); }

bool HapticsManager::init() {
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0)
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            auto* gc = SDL_GameControllerOpen(i);
            if (gc) { haptic_device_ = gc; break; }
        }
    }
    return true;
}

void HapticsManager::shutdown() {
    if (haptic_device_) {
        SDL_GameControllerClose((SDL_GameController*)haptic_device_);
        haptic_device_ = nullptr;
    }
}

bool HapticsManager::supports_vibration() const          { return haptic_device_ != nullptr; }
bool HapticsManager::supports_rgb() const                { return rgb_device_ != nullptr; }
bool HapticsManager::supports_adaptive_triggers() const  { return false; }

void HapticsManager::trigger_event(const HapticEvent& e) {
    if (!enabled_) return;
    if (e.vibration)    trigger_vibration(e.vibrate_intensity, e.vibrate_duration_ms);
    if (e.light_change) trigger_light(e.color, e.light_mode, e.light_duration_ms);
}

void HapticsManager::trigger_vibration(VibrationIntensity intensity, int duration_ms) {
    if (!enabled_ || !haptic_device_) return;
    Uint16 lo = 0, hi = 0;
    switch (intensity) {
        case VibrationIntensity::Weak:   lo = 0x4000; hi = 0x2000; break;
        case VibrationIntensity::Medium: lo = 0x8000; hi = 0x6000; break;
        case VibrationIntensity::Strong: lo = 0xFFFF; hi = 0xC000; break;
    }
    SDL_GameControllerRumble((SDL_GameController*)haptic_device_, lo, hi, (Uint32)duration_ms);
}

void HapticsManager::trigger_light(RGBColor c, LightMode m, int) {
    current_color_ = c; current_light_mode_ = m;
    // Real RGB device hookup is platform-dependent; stub for now.
}

void HapticsManager::on_explosion()    { trigger_vibration(VibrationIntensity::Strong, 200); }
void HapticsManager::on_landing()      { trigger_vibration(VibrationIntensity::Medium, 100); }
void HapticsManager::on_hit()          { trigger_light({255,0,0}, LightMode::Pulse, 500); }
void HapticsManager::on_item_collect() { trigger_light({0,255,0}, LightMode::Pulse, 1000); }
void HapticsManager::on_low_health()   { trigger_light({255,0,0}, LightMode::Breathing, 2000); }
void HapticsManager::on_boss_scene()   { trigger_vibration(VibrationIntensity::Strong, 500); }

void HapticsManager::set_trigger_resistance(float, float) {}
void HapticsManager::apply_vibration(VibrationIntensity, int) {}
void HapticsManager::apply_rgb(RGBColor c, LightMode, int) {
    if (rgb_cb_) rgb_cb_(c);
}
void HapticsManager::apply_trigger_resistance(float, float) {}

RGBColor HapticsManager::compute_dominant_color(const uint8_t* px) const {
    if (!px) return current_color_;
    // Down-sample to a 16x16 grid, find the cell with the highest weighted
    // brightness * saturation, return its average color. Cheap and effective
    // for screen-color RGB lighting.
    constexpr int GW = 16, GH = 16;
    constexpr int CW = 256 / GW, CH = 240 / GH;
    int best_score = -1;
    int best_r = 128, best_g = 128, best_b = 128;
    for (int gy = 0; gy < GH; ++gy) {
        for (int gx = 0; gx < GW; ++gx) {
            int sr = 0, sg = 0, sb = 0;
            for (int y = 0; y < CH; ++y) {
                for (int x = 0; x < CW; ++x) {
                    int p = ((gy*CH + y)*256 + (gx*CW + x))*4;
                    sr += px[p+0]; sg += px[p+1]; sb += px[p+2];
                }
            }
            int n = CW * CH;
            sr /= n; sg /= n; sb /= n;
            int max_c = std::max(sr, std::max(sg, sb));
            int min_c = std::min(sr, std::min(sg, sb));
            int sat   = max_c - min_c;
            int score = max_c + sat * 2;
            if (score > best_score) {
                best_score = score;
                best_r = sr; best_g = sg; best_b = sb;
            }
        }
    }
    return RGBColor{(uint8_t)best_r, (uint8_t)best_g, (uint8_t)best_b};
}

void HapticsManager::update_from_frame(const uint8_t* px) {
    if (!enabled_) return;
    auto c = compute_dominant_color(px);
    current_color_ = c;
    apply_rgb(c, current_light_mode_, 16);
}

} // namespace fcemu
