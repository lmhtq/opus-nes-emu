// include/fcemu/ui.h
#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <vector>
#include <map>
#include <array>
#include <memory>

#include "fcemu/overlay.h"
#include "fcemu/menu.h"

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
    // Player 1
    bool a=false,b=false,select=false,start=false;
    bool up=false,down=false,left=false,right=false;
    // Player 1 turbo (auto-fire while held)
    bool a_turbo=false, b_turbo=false;
    // Player 2 (kept compact — same layout as P1)
    bool p2_a=false, p2_b=false, p2_select=false, p2_start=false;
    bool p2_up=false, p2_down=false, p2_left=false, p2_right=false;
    bool p2_a_turbo=false, p2_b_turbo=false;
    // System
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
    void          render_frame(const uint8_t* rgba, int w = 256, int h = 240);
    void          push_audio(const int16_t* samples, int n_samples_stereo);
    InputSnapshot input_snapshot() const { return snap_; }

    bool debug_overlay() const { return debug_overlay_; }
    void set_hud_visible(bool v) { hud_visible_ = v; }
    bool hud_visible() const { return hud_visible_; }

    // Menu / overlay accessors — main.cpp populates the root menu and may
    // post toasts, set HUD lines, etc.
    Overlay&         overlay() { return overlay_; }
    MenuController&  menu()    { return menu_ctrl_; }
    void set_root_menu(std::shared_ptr<Menu> root) { root_menu_ = std::move(root); }
    bool menu_open() const     { return menu_ctrl_.is_open(); }

    // Optional dynamic HUD lines drawn at top-left when hud_visible_ is true.
    void set_hud_lines(std::vector<std::string> lines) { hud_lines_ = std::move(lines); }

    // Frame timing accessor used by HUD (updated each render_frame).
    float fps() const { return fps_; }

    // Misc.
    void set_title(const std::string& t);
    void set_state(EmuState s);
    EmuState state() const { return state_; }

    // Settings (in-memory + simple ini persistence).
    void        set_setting(const std::string& key, const std::string& value);
    std::string get_setting(const std::string& key) const;
    void        clear_setting(const std::string& key);
    bool save_settings(const std::string& path);
    bool load_settings(const std::string& path);

    // Fill settings_ with default key bindings for any missing "key.*" entry.
    // Idempotent — call after load_settings to ensure menu always has values.
    void seed_default_bindings();
    // Wipe every "key.<action>" entry, then re-seed from defaults.
    void reset_default_bindings();

    // List of all rebindable action IDs (e.g. "p1.a") for menu construction.
    static const std::vector<std::string>& action_ids();

    // If `key_name` is currently bound to some action other than `except_action`,
    // returns that action id; otherwise returns "". Comparison is case-normalized
    // through SDL's key-name table.
    std::string find_action_for_key(const std::string& key_name,
                                    const std::string& except_action = {}) const;
    // Returns all (action, key_name) pairs whose key collides with another action.
    std::vector<std::pair<std::string, std::string>> find_key_conflicts() const;

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
    int   tex_w_ = 256, tex_h_ = 240;
    bool  debug_overlay_ = false;
    bool  hud_visible_   = false;
    uint32_t audio_device_ = 0;
    void* gamepads_[4] = {nullptr,nullptr,nullptr,nullptr}; // SDL_GameController*

    InputSnapshot snap_{};
    std::map<std::string, std::string> settings_;
    RomLoadCallback rom_cb_;
    StateCallback   state_cb_;

    // Overlay + menu state.
    Overlay         overlay_;
    MenuController  menu_ctrl_;
    std::shared_ptr<Menu> root_menu_;
    std::vector<std::string> hud_lines_;
    uint64_t last_render_ticks_ = 0;
    float    fps_ = 60.0f;
    std::vector<uint8_t> overlay_buf_;  // composited working buffer
};

} // namespace fcemu
