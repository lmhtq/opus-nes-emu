// haptics.cpp - Haptics manager (stub)
#include "fcemu/haptics.h"
#include <cstdio>

namespace fcemu {

HapticsManager::HapticsManager()
    : haptic_device_(nullptr), rgb_device_(nullptr), enabled_(true),
      current_color_{255, 255, 255}, current_light_mode_(LightMode::Static) {}

HapticsManager::~HapticsManager() { shutdown(); }

bool HapticsManager::init() {
    printf("HapticsManager: Initializing...\n");
    // TODO: Init SDL_Haptic, RGB devices
    return true;
}

void HapticsManager::shutdown() {
    printf("HapticsManager: Shutdown\n");
}

bool HapticsManager::supports_vibration() const {
    return haptic_device_ != nullptr;
}

bool HapticsManager::supports_rgb() const {
    return rgb_device_ != nullptr;
}

bool HapticsManager::supports_adaptive_triggers() const {
    return false;  // TODO
}

void HapticsManager::trigger_event(const HapticEvent& event) {
    if (!enabled_) return;
    if (event.vibration) trigger_vibration(event.vibrate_intensity, event.vibrate_duration_ms);
    if (event.light_change) trigger_light(event.color, event.light_mode, event.light_duration_ms);
}

void HapticsManager::trigger_vibration(VibrationIntensity intensity, int duration_ms) {
    if (!enabled_) return;
    printf("HapticsManager: Vibration %d for %d ms\n",
           static_cast<int>(intensity), duration_ms);
    // TODO: SDL_HapticRumblePlay
}

void HapticsManager::trigger_light(RGBColor color, LightMode mode, int duration_ms) {
    if (!enabled_) return;
    current_color_ = color;
    current_light_mode_ = mode;
    printf("HapticsManager: Light RGB(%d,%d,%d) mode %d for %d ms\n",
           color.r, color.g, color.b, static_cast<int>(mode), duration_ms);
    // TODO: Set RGB device color
}

void HapticsManager::on_explosion() {
    trigger_vibration(VibrationIntensity::Strong, 200);
}

void HapticsManager::on_landing() {
    trigger_vibration(VibrationIntensity::Medium, 100);
}

void HapticsManager::on_hit() {
    trigger_light({255, 0, 0}, LightMode::Pulse, 500);
}

void HapticsManager::on_item_collect() {
    trigger_light({0, 255, 0}, LightMode::Pulse, 1000);
}

void HapticsManager::on_low_health() {
    trigger_light({255, 0, 0}, LightMode::Breathing, 2000);
}

void HapticsManager::on_boss_scene() {
    trigger_vibration(VibrationIntensity::Strong, 500);
}

void HapticsManager::set_trigger_resistance(float left, float right) {
    printf("HapticsManager: Trigger resistance L=%.1f R=%.1f\n", left, right);
    // TODO: XInput/DirectInput adaptive triggers
}

void HapticsManager::apply_vibration(VibrationIntensity intensity, int duration_ms) { /* TODO */ }
void HapticsManager::apply_rgb(RGBColor color, LightMode mode, int duration_ms) { /* TODO */ }
void HapticsManager::apply_trigger_resistance(float left, float right) { /* TODO */ }

}  // namespace fcemu
