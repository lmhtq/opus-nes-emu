// ui.cpp - SDL2-based window + renderer + audio queue + keyboard/gamepad input.
#include "fcemu/ui.h"

#include <SDL.h>
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace fcemu {

UI::UI() = default;
UI::~UI() { shutdown(); }

// ---- Default keyboard bindings (overridden by load_settings) ----------------
//   Player 1: arrows + Z/X (A/B) + RShift/Return (Select/Start)
//   Player 2: WASD + G/H (A/B) + V/B (Select/Start)
//   Turbo:    A=key_a (P1), B=key_s (P1); J (P2-A turbo), K (P2-B turbo)
struct KeyBinding { SDL_Keycode key; void (*apply)(InputSnapshot&, bool); };

static InputSnapshot g_snap_defaults{};

static void k_p1_a(InputSnapshot& s, bool d){ s.a = d; }
static void k_p1_b(InputSnapshot& s, bool d){ s.b = d; }
static void k_p1_sel(InputSnapshot& s, bool d){ s.select = d; }
static void k_p1_start(InputSnapshot& s, bool d){ s.start = d; }
static void k_p1_up(InputSnapshot& s, bool d){ s.up = d; }
static void k_p1_dn(InputSnapshot& s, bool d){ s.down = d; }
static void k_p1_lf(InputSnapshot& s, bool d){ s.left = d; }
static void k_p1_rt(InputSnapshot& s, bool d){ s.right = d; }
static void k_p1_at(InputSnapshot& s, bool d){ s.a_turbo = d; }
static void k_p1_bt(InputSnapshot& s, bool d){ s.b_turbo = d; }

static void k_p2_a(InputSnapshot& s, bool d){ s.p2_a = d; }
static void k_p2_b(InputSnapshot& s, bool d){ s.p2_b = d; }
static void k_p2_sel(InputSnapshot& s, bool d){ s.p2_select = d; }
static void k_p2_start(InputSnapshot& s, bool d){ s.p2_start = d; }
static void k_p2_up(InputSnapshot& s, bool d){ s.p2_up = d; }
static void k_p2_dn(InputSnapshot& s, bool d){ s.p2_down = d; }
static void k_p2_lf(InputSnapshot& s, bool d){ s.p2_left = d; }
static void k_p2_rt(InputSnapshot& s, bool d){ s.p2_right = d; }
static void k_p2_at(InputSnapshot& s, bool d){ s.p2_a_turbo = d; }
static void k_p2_bt(InputSnapshot& s, bool d){ s.p2_b_turbo = d; }

// Action name → setter, for ini-driven remapping.
static const std::unordered_map<std::string, void(*)(InputSnapshot&, bool)>& action_table() {
    static const std::unordered_map<std::string, void(*)(InputSnapshot&, bool)> t = {
        {"p1.a",k_p1_a},{"p1.b",k_p1_b},{"p1.select",k_p1_sel},{"p1.start",k_p1_start},
        {"p1.up",k_p1_up},{"p1.down",k_p1_dn},{"p1.left",k_p1_lf},{"p1.right",k_p1_rt},
        {"p1.a_turbo",k_p1_at},{"p1.b_turbo",k_p1_bt},
        {"p2.a",k_p2_a},{"p2.b",k_p2_b},{"p2.select",k_p2_sel},{"p2.start",k_p2_start},
        {"p2.up",k_p2_up},{"p2.down",k_p2_dn},{"p2.left",k_p2_lf},{"p2.right",k_p2_rt},
        {"p2.a_turbo",k_p2_at},{"p2.b_turbo",k_p2_bt},
    };
    return t;
}

// Defaults applied when no settings exist.
static std::unordered_map<SDL_Keycode, void(*)(InputSnapshot&, bool)> default_keymap() {
    return {
        // Player 1: arrows + Z/X (A/B), RShift/Return (Sel/Start), A/S turbo.
        {SDLK_z, k_p1_a},      {SDLK_x, k_p1_b},
        {SDLK_RSHIFT, k_p1_sel},{SDLK_RETURN, k_p1_start},
        {SDLK_UP, k_p1_up},    {SDLK_DOWN, k_p1_dn},
        {SDLK_LEFT, k_p1_lf},  {SDLK_RIGHT, k_p1_rt},
        {SDLK_a, k_p1_at},     {SDLK_s, k_p1_bt},
        // Player 2: IJKL (DPad) + G/H (A/B), V/B (Sel/Start), T/Y turbo.
        {SDLK_i, k_p2_up},     {SDLK_k, k_p2_dn},
        {SDLK_j, k_p2_lf},     {SDLK_l, k_p2_rt},
        {SDLK_g, k_p2_a},      {SDLK_h, k_p2_b},
        {SDLK_v, k_p2_sel},    {SDLK_b, k_p2_start},
        {SDLK_t, k_p2_at},     {SDLK_y, k_p2_bt},
    };
}

bool UI::init(const WindowConfig& cfg) {
    cfg_ = cfg;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
#if defined(__APPLE__)
    // macOS：优先使用 Metal renderer（SDL2 自带），便于 AI 超分输出大纹理上传。
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
#endif
    auto* win = SDL_CreateWindow(cfg_.title.c_str(),
                                 SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 cfg_.width, cfg_.height,
                                 SDL_WINDOW_SHOWN |
                                 (cfg_.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
    if (!win) { std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return false; }
    window_ = win;

    auto* ren = SDL_CreateRenderer(win, -1,
                                   SDL_RENDERER_ACCELERATED |
                                   (cfg_.vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
#if defined(__APPLE__)
    if (!ren) {
        // metal 不可用时（如 dummy video driver / headless）回退到 software。
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    }
#endif
    if (!ren) { std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return false; }
    renderer_ = ren;
    {
        SDL_RendererInfo info{};
        if (SDL_GetRendererInfo(ren, &info) == 0)
            std::printf("[ui] SDL renderer: %s\n", info.name ? info.name : "?");
    }
    SDL_RenderSetLogicalSize(ren, 256, 240);

    auto* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA32,
                                  SDL_TEXTUREACCESS_STREAMING, 256, 240);
    if (!tex) { std::fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError()); return false; }
    texture_ = tex;

    SDL_AudioSpec want{}, have{};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    audio_device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (audio_device_ != 0) SDL_PauseAudioDevice(audio_device_, 0);

    // Open up to 4 gamepads (auto-detect).
    int n = SDL_NumJoysticks();
    int slot = 0;
    for (int i = 0; i < n && slot < 4; ++i) {
        if (SDL_IsGameController(i)) {
            SDL_GameController* gc = SDL_GameControllerOpen(i);
            if (gc) {
                gamepads_[slot++] = gc;
                std::printf("Gamepad %d connected: %s\n", slot - 1, SDL_GameControllerName(gc));
            }
        }
    }

    state_ = EmuState::Running;
    return true;
}

void UI::shutdown() {
    for (int i = 0; i < 4; ++i) {
        if (gamepads_[i]) { SDL_GameControllerClose((SDL_GameController*)gamepads_[i]); gamepads_[i] = nullptr; }
    }
    if (audio_device_) { SDL_CloseAudioDevice(audio_device_); audio_device_ = 0; }
    if (texture_)  { SDL_DestroyTexture((SDL_Texture*)texture_); texture_ = nullptr; }
    if (renderer_) { SDL_DestroyRenderer((SDL_Renderer*)renderer_); renderer_ = nullptr; }
    if (window_)   { SDL_DestroyWindow((SDL_Window*)window_); window_ = nullptr; }
    if (SDL_WasInit(0)) SDL_Quit();
}

// Build effective keyboard map: defaults overlaid by ini "key.<action>=<sdlk>".
static std::unordered_map<SDL_Keycode, void(*)(InputSnapshot&, bool)>
build_keymap(const std::map<std::string, std::string>& settings) {
    auto km = default_keymap();
    // Allow ini overrides like:
    //   key.p1.a=z   key.p1.b=x   key.p1.up=up   key.p1.a_turbo=a
    // value can be either an SDL_Keycode int or an SDL key name (e.g. "Z","Up","Return").
    //
    // Apply order matters when two actions claim the same key. std::map
    // iterates alphabetically, which would let p2.* overwrite p1.* (e.g.
    // a user who rebinds p1.b=H ends up with H actually triggering p2.b
    // because p2.b=H is the seeded default and gets applied later). Player
    // 1 is the primary controller, so we apply p1 LAST and let it win.
    std::vector<const std::pair<const std::string, std::string>*> ordered;
    ordered.reserve(settings.size());
    for (auto& kv : settings) {
        if (kv.first.rfind("key.", 0) == 0) ordered.push_back(&kv);
    }
    std::stable_sort(ordered.begin(), ordered.end(),
        [](const auto* a, const auto* b) {
            // p2.* < p1.* < everything else: lower rank applied first, last writer wins.
            auto rank = [](const std::string& s) -> int {
                if (s.rfind("key.p2.", 0) == 0) return 0;
                if (s.rfind("key.p1.", 0) == 0) return 1;
                return 2;
            };
            return rank(a->first) < rank(b->first);
        });
    for (auto* pkv : ordered) {
        auto& kv = *pkv;
        const std::string action = kv.first.substr(4);
        auto it = action_table().find(action);
        if (it == action_table().end()) continue;
        SDL_Keycode k = SDLK_UNKNOWN;
        try { k = (SDL_Keycode)std::stoi(kv.second); }
        catch (...) { k = SDL_GetKeyFromName(kv.second.c_str()); }
        if (k == SDLK_UNKNOWN) continue;
        // remove any previous binding of this action
        for (auto m = km.begin(); m != km.end(); ) {
            if (m->second == it->second) m = km.erase(m); else ++m;
        }
        // Last writer wins on key collision (p1 beats p2 thanks to the
        // ordering above). The displaced action becomes unbound; the UI
        // surfaces this via UI::find_key_conflicts() at startup / on demand.
        km[k] = it->second;
    }
    return km;
}

static void apply_gamepad_button(InputSnapshot& s, int player, SDL_GameControllerButton b, bool down) {
    auto set_p1 = [&](void(*f)(InputSnapshot&,bool)){ f(s, down); };
    auto set_p2 = [&](void(*f)(InputSnapshot&,bool)){ f(s, down); };
    // Convention: NES A = right face button (SDL B); NES B = bottom face (SDL A).
    switch (b) {
        case SDL_CONTROLLER_BUTTON_B:        player==0 ? set_p1(k_p1_a)     : set_p2(k_p2_a); break;
        case SDL_CONTROLLER_BUTTON_A:        player==0 ? set_p1(k_p1_b)     : set_p2(k_p2_b); break;
        case SDL_CONTROLLER_BUTTON_Y:        player==0 ? set_p1(k_p1_at)    : set_p2(k_p2_at); break; // turbo A
        case SDL_CONTROLLER_BUTTON_X:        player==0 ? set_p1(k_p1_bt)    : set_p2(k_p2_bt); break; // turbo B
        case SDL_CONTROLLER_BUTTON_BACK:     player==0 ? set_p1(k_p1_sel)   : set_p2(k_p2_sel); break;
        case SDL_CONTROLLER_BUTTON_START:    player==0 ? set_p1(k_p1_start) : set_p2(k_p2_start); break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:    player==0 ? set_p1(k_p1_up)  : set_p2(k_p2_up); break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:  player==0 ? set_p1(k_p1_dn)  : set_p2(k_p2_dn); break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:  player==0 ? set_p1(k_p1_lf)  : set_p2(k_p2_lf); break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: player==0 ? set_p1(k_p1_rt)  : set_p2(k_p2_rt); break;
        case SDL_CONTROLLER_BUTTON_GUIDE:    if (down) s.reset = true; break;
        default: break;
    }
}

static void apply_gamepad_axes(InputSnapshot& s, int player, SDL_GameController* gc) {
    // Treat the left analog stick as a virtual D-pad (deadzone 8000/32768).
    constexpr int DZ = 8000;
    int x = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTX);
    int y = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_LEFTY);
    bool left  = x < -DZ, right = x > DZ;
    bool up    = y < -DZ, down  = y > DZ;
    if (player == 0) {
        if (left)  s.left = true;
        if (right) s.right = true;
        if (up)    s.up = true;
        if (down)  s.down = true;
    } else {
        if (left)  s.p2_left = true;
        if (right) s.p2_right = true;
        if (up)    s.p2_up = true;
        if (down)  s.p2_down = true;
    }
}

void UI::process_events() {
    snap_.save_state = snap_.load_state = snap_.reset = false;
    auto km = build_keymap(settings_);

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT: quit_ = true; break;
            case SDL_KEYDOWN: {
                SDL_Keycode k = ev.key.keysym.sym;

                // Menu keybind capture takes priority — bind the next key.
                if (menu_ctrl_.is_capturing_key()) {
                    if (k == SDLK_ESCAPE) { menu_ctrl_.on_key(MenuKey::Back); break; }
                    const char* name = SDL_GetKeyName(k);
                    if (name && *name) menu_ctrl_.capture_key_name(name);
                    break;
                }

                // While the menu is open it owns navigation keys.
                if (menu_ctrl_.is_open()) {
                    switch (k) {
                        case SDLK_ESCAPE: menu_ctrl_.on_key(MenuKey::Back); break;
                        case SDLK_UP:     menu_ctrl_.on_key(MenuKey::Up); break;
                        case SDLK_DOWN:   menu_ctrl_.on_key(MenuKey::Down); break;
                        case SDLK_LEFT:   menu_ctrl_.on_key(MenuKey::Left); break;
                        case SDLK_RIGHT:  menu_ctrl_.on_key(MenuKey::Right); break;
                        case SDLK_RETURN:
                        case SDLK_SPACE:  menu_ctrl_.on_key(MenuKey::Activate); break;
                        case SDLK_BACKSPACE: menu_ctrl_.on_key(MenuKey::Back); break;
                    }
                    break;
                }

                // Global hotkeys (game-mode).
                if (k == SDLK_ESCAPE) {
                    if (root_menu_) menu_ctrl_.open(root_menu_);
                    else quit_ = true;
                    break;
                }
                if (k == SDLK_F1)  { snap_.save_state = true; break; }
                if (k == SDLK_F2)  { snap_.load_state = true; break; }
                if (k == SDLK_F3)  { debug_overlay_ = !debug_overlay_; break; }
                if (k == SDLK_F4)  { if (root_menu_) menu_ctrl_.open(root_menu_); break; }
                if (k == SDLK_F5)  { snap_.reset = true; break; }
                if (k == SDLK_TAB) { hud_visible_ = !hud_visible_; break; }

                auto it = km.find(k);
                if (it != km.end()) it->second(snap_, true);
                break;
            }
            case SDL_KEYUP: {
                if (menu_ctrl_.is_open()) break; // ignore game keys while paused
                auto it = km.find(ev.key.keysym.sym);
                if (it != km.end()) it->second(snap_, false);
                break;
            }
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP: {
                int player = -1;
                SDL_GameController* src = SDL_GameControllerFromInstanceID(ev.cbutton.which);
                for (int i = 0; i < 2; ++i) {
                    if (gamepads_[i] && (SDL_GameController*)gamepads_[i] == src) { player = i; break; }
                }
                if (player < 0) break;
                apply_gamepad_button(snap_, player,
                                     (SDL_GameControllerButton)ev.cbutton.button,
                                     ev.type == SDL_CONTROLLERBUTTONDOWN);
                break;
            }
            case SDL_CONTROLLERDEVICEADDED: {
                if (SDL_IsGameController(ev.cdevice.which)) {
                    for (int i = 0; i < 4; ++i) {
                        if (!gamepads_[i]) {
                            gamepads_[i] = SDL_GameControllerOpen(ev.cdevice.which);
                            if (gamepads_[i])
                                std::printf("Gamepad %d hot-plugged: %s\n", i,
                                            SDL_GameControllerName((SDL_GameController*)gamepads_[i]));
                            break;
                        }
                    }
                }
                break;
            }
            case SDL_CONTROLLERDEVICEREMOVED: {
                for (int i = 0; i < 4; ++i) {
                    if (gamepads_[i] && SDL_JoystickInstanceID(
                            SDL_GameControllerGetJoystick((SDL_GameController*)gamepads_[i]))
                            == ev.cdevice.which) {
                        SDL_GameControllerClose((SDL_GameController*)gamepads_[i]);
                        gamepads_[i] = nullptr;
                        std::printf("Gamepad %d disconnected\n", i);
                        break;
                    }
                }
                break;
            }
        }
    }

    // Poll analog sticks each frame (axis events are noisy; sample directly).
    for (int p = 0; p < 2; ++p) {
        if (gamepads_[p]) apply_gamepad_axes(snap_, p, (SDL_GameController*)gamepads_[p]);
    }
}

void UI::render_frame(const uint8_t* px, int w, int h) {
    if (!renderer_) return;
    if (!texture_ || w != tex_w_ || h != tex_h_) {
        if (texture_) SDL_DestroyTexture((SDL_Texture*)texture_);
        texture_ = SDL_CreateTexture((SDL_Renderer*)renderer_, SDL_PIXELFORMAT_RGBA32,
                                     SDL_TEXTUREACCESS_STREAMING, w, h);
        tex_w_ = w; tex_h_ = h;
        SDL_RenderSetLogicalSize((SDL_Renderer*)renderer_, w, h);
    }
    if (!texture_) return;

    // Composite overlay (toasts, menu, HUD) on top of the game frame.
    overlay_buf_.assign(px, px + (size_t)w * h * 4);
    uint8_t* buf = overlay_buf_.data();

    // Frame timing for HUD.
    Uint64 now = SDL_GetPerformanceCounter();
    if (last_render_ticks_) {
        double dt = (double)(now - last_render_ticks_) / SDL_GetPerformanceFrequency();
        if (dt > 0.0001) {
            float inst = (float)(1.0 / dt);
            fps_ = fps_ * 0.9f + inst * 0.1f;
        }
        overlay_.update((float)dt);
    }
    last_render_ticks_ = now;

    if (hud_visible_) {
        char fps_buf[32];
        std::snprintf(fps_buf, sizeof(fps_buf), "FPS %4.1f", fps_);
        Overlay::fill_rect(buf, w, h, 4, 4, 64, 12, RGBA{0,0,0,160});
        Overlay::draw_text(buf, w, h, 6, 6, fps_buf, Overlay::Green);
        int hy = 20;
        for (auto& line : hud_lines_) {
            int tw = Overlay::text_width(line);
            Overlay::fill_rect(buf, w, h, 4, hy, tw + 6, 10, RGBA{0,0,0,140});
            Overlay::draw_text(buf, w, h, 6, hy + 1, line, Overlay::Cyan);
            hy += 11;
        }
    }

    overlay_.render_toasts(buf, w, h);
    if (menu_ctrl_.is_open()) menu_ctrl_.render(buf, w, h, overlay_);

    SDL_UpdateTexture((SDL_Texture*)texture_, nullptr, buf, w * 4);
    SDL_RenderClear((SDL_Renderer*)renderer_);
    SDL_RenderCopy((SDL_Renderer*)renderer_, (SDL_Texture*)texture_, nullptr, nullptr);
    SDL_RenderPresent((SDL_Renderer*)renderer_);
}

void UI::push_audio(const int16_t* samples, int n_samples_stereo) {
    if (!audio_device_) return;
    if (SDL_GetQueuedAudioSize(audio_device_) > 8192 * 4) return;
    SDL_QueueAudio(audio_device_, samples, (Uint32)(n_samples_stereo * sizeof(int16_t)));
}

void UI::set_title(const std::string& t) {
    cfg_.title = t;
    if (window_) SDL_SetWindowTitle((SDL_Window*)window_, t.c_str());
}

void UI::set_window_size(int w, int h) {
    if (!window_ || w <= 0 || h <= 0) return;
    SDL_Window* win = (SDL_Window*)window_;
    int idx = SDL_GetWindowDisplayIndex(win);
    SDL_Rect db{};
    if (idx >= 0 && SDL_GetDisplayUsableBounds(idx, &db) == 0 && db.w > 0 && db.h > 0) {
        // Leave a little headroom for menu bar / title bar.
        const int max_w = db.w - 40;
        const int max_h = db.h - 80;
        if (w > max_w || h > max_h) {
            double sx = (double)max_w / w;
            double sy = (double)max_h / h;
            double s = sx < sy ? sx : sy;
            w = (int)(w * s);
            h = (int)(h * s);
        }
    }
    SDL_SetWindowSize(win, w, h);
    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    cfg_.width = w; cfg_.height = h;
}

void UI::set_state(EmuState s) {
    state_ = s;
    if (state_cb_) state_cb_(s);
}

void UI::load_rom(const std::string& path) {
    if (rom_cb_) rom_cb_(path);
}

void UI::set_setting(const std::string& key, const std::string& value) { settings_[key] = value; }
void UI::clear_setting(const std::string& key) { settings_.erase(key); }
std::string UI::get_setting(const std::string& key) const {
    auto it = settings_.find(key);
    return it == settings_.end() ? std::string{} : it->second;
}

bool UI::save_settings(const std::string& path) {
    std::ofstream f(path);
    if (!f) return false;
    for (auto& kv : settings_) f << kv.first << "=" << kv.second << "\n";
    return true;
}

bool UI::load_settings(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        settings_[line.substr(0, eq)] = line.substr(eq + 1);
    }
    return true;
}

// ---- Default-binding seeding & action enumeration -------------------------
namespace {
struct DefBind { const char* action; SDL_Keycode key; };
static const DefBind kDefBinds[] = {
    {"p1.a", SDLK_z}, {"p1.b", SDLK_x},
    {"p1.select", SDLK_RSHIFT}, {"p1.start", SDLK_RETURN},
    {"p1.up", SDLK_UP}, {"p1.down", SDLK_DOWN},
    {"p1.left", SDLK_LEFT}, {"p1.right", SDLK_RIGHT},
    {"p1.a_turbo", SDLK_a}, {"p1.b_turbo", SDLK_s},
    {"p2.up", SDLK_i}, {"p2.down", SDLK_k},
    {"p2.left", SDLK_j}, {"p2.right", SDLK_l},
    {"p2.a", SDLK_g}, {"p2.b", SDLK_h},
    {"p2.select", SDLK_v}, {"p2.start", SDLK_b},
    {"p2.a_turbo", SDLK_t}, {"p2.b_turbo", SDLK_y},
};
} // namespace

void UI::seed_default_bindings() {
    for (auto& d : kDefBinds) {
        std::string skey = std::string("key.") + d.action;
        if (settings_.find(skey) == settings_.end()) {
            const char* name = SDL_GetKeyName(d.key);
            if (name && *name) settings_[skey] = name;
        }
    }
}

void UI::reset_default_bindings() {
    for (auto& act : action_ids()) settings_.erase(std::string("key.") + act);
    seed_default_bindings();
}

std::string UI::find_action_for_key(const std::string& key_name,
                                    const std::string& except_action) const {
    SDL_Keycode want = SDL_GetKeyFromName(key_name.c_str());
    if (want == SDLK_UNKNOWN) return {};
    for (auto& kv : settings_) {
        if (kv.first.rfind("key.", 0) != 0) continue;
        std::string act = kv.first.substr(4);
        if (act == except_action) continue;
        SDL_Keycode k = SDL_GetKeyFromName(kv.second.c_str());
        if (k == want) return act;
    }
    return {};
}

std::vector<std::pair<std::string, std::string>> UI::find_key_conflicts() const {
    std::vector<std::pair<std::string, std::string>> out;
    std::map<SDL_Keycode, std::vector<std::string>> by_key;
    for (auto& kv : settings_) {
        if (kv.first.rfind("key.", 0) != 0) continue;
        SDL_Keycode k = SDL_GetKeyFromName(kv.second.c_str());
        if (k == SDLK_UNKNOWN) continue;
        by_key[k].push_back(kv.first.substr(4));
    }
    for (auto& kv : by_key) {
        if (kv.second.size() < 2) continue;
        const char* nm = SDL_GetKeyName(kv.first);
        for (auto& act : kv.second) out.emplace_back(act, nm ? nm : "?");
    }
    return out;
}

const std::vector<std::string>& UI::action_ids() {
    static std::vector<std::string> v = []{
        std::vector<std::string> r;
        for (auto& d : kDefBinds) r.push_back(d.action);
        return r;
    }();
    return v;
}

} // namespace fcemu
