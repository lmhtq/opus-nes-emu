// include/fcemu/ui.h
#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <map>
#include <array>

namespace fcemu {

struct WindowConfig {
    int   width      = 768;
    int   height     = 720;
    bool  fullscreen = false;
    bool  vsync      = true;
    float scale      = 3.0f;
    std::string title = "fcemu";
};

enum class EmuState { Stopped, Running, Paused, StepFrame };

struct InputSnapshot {
    bool a=false,b=false,select=false,start=false;
    bool up=false,down=false,left=false,right=false;
    bool quit=false;
    bool reset=false;
    bool save_state=false;
    bool load_state=false;
};

using RomLoadCallback = std::function<void(const std::string&)>;
using StateCallback   = std::function<void(EmuState)>;

class UI {
public:
    UI();
    ~UI();

    bool init(const WindowConfig& config);
    void shutdown();

    // Per-frame ops driven by main loop.
    void          process_events();
    bool          should_quit() const { return quit_; }
    void          render_frame(const uint8_t* rgba_256x240);
    void          push_audio(const int16_t* samples, int n_samples_stereo);
    InputSnapshot input_snapshot() const { return snap_; }

    // Misc.
    void set_title(const std::string& t);
    void set_state(EmuState s);
    EmuState state() const { return state_; }

    // Settings (in-memory + simple ini persistence).
    void        set_setting(const std::string& key, const std::string& value);
    std::string get_setting(const std::string& key) const;
    bool save_settings(const std::string& path);
    bool load_settings(const std::string& path);

    void set_rom_load_callback(RomLoadCallback cb) { rom_cb_ = std::move(cb); }
    void set_state_callback(StateCallback cb) { state_cb_ = std::move(cb); }
    void load_rom(const std::string& path);

private:
    WindowConfig cfg_;
    EmuState     state_ = EmuState::Stopped;
    bool         quit_  = false;

    void* window_   = nullptr;     // SDL_Window*
    void* renderer_ = nullptr;     // SDL_Renderer*
    void* texture_  = nullptr;     // SDL_Texture*
    uint32_t audio_device_ = 0;

    InputSnapshot snap_{};
    std::map<std::string, std::string> settings_;
    RomLoadCallback rom_cb_;
    StateCallback   state_cb_;
};

} // namespace fcemu
