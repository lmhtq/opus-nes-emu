// include/fcemu/haptics.h
#pragma once

#include <cstdint>
#include <string>

namespace fcemu {

enum class VibrationIntensity { Weak, Medium, Strong };

struct RGBColor { uint8_t r, g, b; };

enum class LightMode { Static, Breathing, Pulse, Wave };

struct HapticEvent {
    bool vibration = false;
    VibrationIntensity vibrate_intensity = VibrationIntensity::Medium;
    int vibrate_duration_ms = 200;
    bool light_change = false;
    RGBColor color = {255, 255, 255};
    LightMode light_mode = LightMode::Static;
    int light_duration_ms = 500;
};

class HapticsManager {
public:
    HapticsManager();
    ~HapticsManager();
    bool init();
    void shutdown();
    bool supports_vibration() const;
    bool supports_rgb() const;
    bool supports_adaptive_triggers() const;
    void trigger_event(const HapticEvent& event);
    void trigger_vibration(VibrationIntensity intensity, int duration_ms);
    void trigger_light(RGBColor color, LightMode mode, int duration_ms);
    void on_explosion();
    void on_landing();
    void on_hit();
    void on_item_collect();
    void on_low_health();
    void on_boss_scene();
    void set_trigger_resistance(float left_resistance, float right_resistance);
    void set_enabled(bool enable) { enabled_ = enable; }
    bool enabled() const { return enabled_; }

private:
    void* haptic_device_;
    void* rgb_device_;
    bool enabled_ = true;
    RGBColor current_color_;
    LightMode current_light_mode_;
    void apply_vibration(VibrationIntensity intensity, int duration_ms);
    void apply_rgb(RGBColor color, LightMode mode, int duration_ms);
    void apply_trigger_resistance(float left, float right);
};

} // namespace fcemu
