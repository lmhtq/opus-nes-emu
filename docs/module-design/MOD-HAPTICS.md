# MOD-HAPTICS: 触觉反馈与 RGB 灯光

## 元数据 (Metadata)

- **ID**: MOD-HAPTICS
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-111, REQ-112
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 功能职责 (Responsibilities)

实现手柄触觉反馈和 RGB 灯光同步。

核心职责：
1. 震动映射（爆炸/碰撞/着陆等场景）
2. 震动强度和持续时间可调
3. 自适应扳机阻力模拟
4. 支持主流手柄（Xbox/PlayStation/Switch Pro）
5. 血量感知（血量少时红色脉冲）
6. 道具收集（收集时绿色闪烁）
7. 受伤反馈（受伤时红色闪烁）
8. 场景氛围灯光
9. 预设灯光模式（呼吸/脉冲/波浪）
10. RGB 同步可开关

## 接口设计 (Interface Design)

```cpp
// include/fcemu/haptics.h
#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace fcemu {

// 震动强度
enum class VibrationIntensity { Weak, Medium, Strong };

// RGB 颜色
struct RGBColor {
    uint8_t r, g, b;
};

// 灯光模式
enum class LightMode { Static, Breathing, Pulse, Wave };

// 触觉/灯光事件
struct HapticEvent {
    // 震动
    bool vibration = false;
    VibrationIntensity vibrate_intensity = VibrationIntensity::Medium;
    int vibrate_duration_ms = 200;

    // RGB 灯光
    bool light_change = false;
    RGBColor color = {255, 255, 255};
    LightMode light_mode = LightMode::Static;
    int light_duration_ms = 500;
};

// 触觉管理器
class HapticsManager {
public:
    HapticsManager();
    ~HapticsManager();

    bool init();
    void shutdown();

    // 手柄支持检测
    bool supports_vibration() const;
    bool supports_rgb() const;
    bool supports_adaptive_triggers() const;

    // 触发事件
    void trigger_event(const HapticEvent& event);
    void trigger_vibration(VibrationIntensity intensity, int duration_ms);
    void trigger_light(RGBColor color, LightMode mode, int duration_ms);

    // 场景预设
    void on_explosion();       // 爆炸
    void on_landing();         // 着陆
    void on_hit();            // 受伤
    void on_item_collect();    // 收集道具
    void on_low_health();      // 低血量
    void on_boss_scene();      // Boss 场景

    // 自适应扳机
    void set_trigger_resistance(float left_resistance, float right_resistance);
    // 0.0 = no resistance, 1.0 = max resistance

    // 开关
    void set_enabled(bool enable) { enabled_ = enable; }
    bool enabled() const { return enabled_; }

private:
    // 平台相关（SDL2 / XInput / DirectInput）
    void* haptic_device_;       // SDL_Haptic*
    void* rgb_device_;          // 平台 RGB 控制器
    bool enabled_ = true;

    // 内部状态
    RGBColor current_color_;
    LightMode current_light_mode_;

    // 辅助
    void apply_vibration(VibrationIntensity intensity, int duration_ms);
    void apply_rgb(RGBColor color, LightMode mode, int duration_ms);
    void apply_trigger_resistance(float left, float right);
};

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-INPUT | 获取手柄设备信息 |
| MOD-UI | 触觉/灯光设置界面 |
| MOD-RESOURCE | 场景检测（用于自动触发） |

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/input/controllers.md` - 输入设备参考

## 变更记录 (Change History)

- 2026-04-30: Initial version
