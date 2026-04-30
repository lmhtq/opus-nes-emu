# MOD-UI: UI 与窗口管理

## 元数据 (Metadata)

- **ID**: MOD-UI
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-006
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 功能职责 (Responsibilities)

实现模拟器的用户界面和窗口管理系统。

核心职责：
1. 创建应用窗口（SDL2 + OpenGL）
2. 显示 PPU 渲染输出（256x240，可缩放）
3. 菜单栏（文件、设置、帮助等）
4. ROM 文件打开对话框
5. 模拟器控制（运行/暂停/重置/逐帧）
6. 速度控制（加速/减速/正常）
7. 全屏切换
8. 音频输出设备选择
9. 设置界面（键位、音频、视频等配置）
10. 即时存档（多个槽位）

## 接口设计 (Interface Design)

```cpp
// include/fcemu/ui.h
#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace fcemu {

// 窗口设置
struct WindowConfig {
    int width = 512;
    int height = 480;
    bool fullscreen = false;
    bool vsync = true;
    float scale = 2.0f;  // 缩放倍数
};

// 模拟器状态
enum class EmuState {
    Stopped,
    Running,
    Paused,
    StepFrame  // 逐帧
};

// UI 事件回调
using RomLoadCallback = std::function<void(const std::string& path)>;
using StateCallback = std::function<void(EmuState state)>;
using SettingsCallback = std::function<void(const std::string& key, const std::string& value)>;

class UI {
public:
    UI();
    ~UI();

    bool init(const WindowConfig& config);
    void shutdown();

    // 主循环
    void run();
    void process_events();

    // 渲染
    void render_frame(const uint8_t* framebuffer);  // RGBA
    void set_render_scale(float scale);

    // 模拟器控制
    void load_rom(const std::string& path);
    void set_state(EmuState state);
    EmuState state() const { return state_; }

    // 设置
    void save_settings();
    void load_settings();
    void set_setting(const std::string& key, const std::string& value);
    std::string get_setting(const std::string& key) const;

    // 即时存档
    bool save_state(int slot);
    bool load_state(int slot);

    // 回调注册
    void set_rom_load_callback(RomLoadCallback cb);
    void set_state_callback(StateCallback cb);

private:
    WindowConfig config_;
    EmuState state_;
    void* window_;        // SDL_Window*
    void* gl_context_;    // SDL_GLContext
    uint32_t texture_id_; // OpenGL texture

    RomLoadCallback rom_load_callback_;
    StateCallback state_callback_;

    // Dear ImGui
    void init_imgui();
    void shutdown_imgui();
    void render_imgui();

    // 菜单/设置
    void render_main_menu();
    void render_settings_window();
    void render_debug_windows();
};

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-CPU | UI 控制模拟器状态 |
| MOD-PPU | UI 渲染 PPU 输出 |
| MOD-APU | UI 选择音频设备 |
| MOD-INPUT | UI 传递输入事件 |
| MOD-VIDEO | UI 叠加视频增强效果 |
| MOD-AUDIO | UI 控制音频增强参数 |
| MOD-HAPTICS | UI 控制触觉反馈开关 |
| MOD-SOCIAL | UI 控制直播互动 |
| MOD-REPLAY | UI 控制回放录制 |

## 数据结构 (Data Structures)

### 设置存储

```cpp
struct Settings {
    // 视频
    float render_scale;
    bool vsync;
    bool fullscreen;

    // 音频
    int sample_rate;
    int audio_device;

    // 输入
    std::map<int, int> key_mapping;  // SDL key -> Button

    // 增强功能
    bool video_enhancements;
    bool audio_enhancements;
    bool haptics_enabled;
    bool social_enabled;
    bool replay_enabled;
};
```

## 关联硬件文档 (Related Hardware Docs)

- 无（UI 是模拟器自身功能）

## 变更记录 (Change History)

- 2026-04-30: Initial version
