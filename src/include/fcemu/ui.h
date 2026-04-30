// include/fcemu/ui.h"
#pragma once

#include <string>
#include <functional>

namespace fcemu {

struct WindowConfig {
    int width = 512;
    int height = 480;
    bool fullscreen = false;
    bool vsync = true;
    float scale = 2.0f;
};

enum class EmuState { Stopped, Running, Paused, StepFrame };

using RomLoadCallback = std::function<void(const std::string&)>;
using StateCallback = std::function<void(EmuState)>;

class UI {
public:
    UI();
    ~UI();
    bool init(const WindowConfig& config);
    void shutdown();
    void run();
    void process_events();
    void render_frame(const uint8_t* framebuffer);
    void set_render_scale(float scale);
    void load_rom(const std::string& path);
    void set_state(EmuState state);
    EmuState state() const { return state_; }
    bool save_state(int slot);
    bool load_state(int slot);
    void set_rom_load_callback(RomLoadCallback cb) { rom_load_callback_ = cb; }
    void set_state_callback(StateCallback cb) { state_callback_ = cb; }
    void set_setting(const std::string& key, const std::string& value);
    std::string get_setting(const std::string& key) const;
    void save_settings();
    void load_settings();

private:
    WindowConfig config_;
    EmuState state_;
    void* window_;
    void* gl_context_;
    uint32_t texture_id_;
    RomLoadCallback rom_load_callback_;
    StateCallback state_callback_;
    void init_imgui();
    void shutdown_imgui();
    void render_imgui();
    void render_main_menu();
    void render_settings_window();
};

} // namespace fcemu
