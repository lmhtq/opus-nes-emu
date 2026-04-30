// ui.cpp - SDL2-based window + renderer + audio queue.
#include "fcemu/ui.h"

#include <SDL.h>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace fcemu {

UI::UI() = default;
UI::~UI() { shutdown(); }

bool UI::init(const WindowConfig& cfg) {
    cfg_ = cfg;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
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
    if (!ren) { std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return false; }
    renderer_ = ren;
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

    state_ = EmuState::Running;
    return true;
}

void UI::shutdown() {
    if (audio_device_) { SDL_CloseAudioDevice(audio_device_); audio_device_ = 0; }
    if (texture_)  { SDL_DestroyTexture((SDL_Texture*)texture_); texture_ = nullptr; }
    if (renderer_) { SDL_DestroyRenderer((SDL_Renderer*)renderer_); renderer_ = nullptr; }
    if (window_)   { SDL_DestroyWindow((SDL_Window*)window_); window_ = nullptr; }
    if (SDL_WasInit(0)) SDL_Quit();
}

static void apply_key(InputSnapshot& s, SDL_Keycode k, bool down) {
    switch (k) {
        case SDLK_z:      s.a = down; break;
        case SDLK_x:      s.b = down; break;
        case SDLK_RSHIFT: s.select = down; break;
        case SDLK_RETURN: s.start = down; break;
        case SDLK_UP:     s.up = down; break;
        case SDLK_DOWN:   s.down = down; break;
        case SDLK_LEFT:   s.left = down; break;
        case SDLK_RIGHT:  s.right = down; break;
        case SDLK_F1:     if (down) s.save_state = true; break;
        case SDLK_F2:     if (down) s.load_state = true; break;
        case SDLK_F5:     if (down) s.reset = true; break;
        default: break;
    }
}

void UI::process_events() {
    snap_.save_state = snap_.load_state = snap_.reset = false;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_QUIT: quit_ = true; break;
            case SDL_KEYDOWN:
                if (ev.key.keysym.sym == SDLK_ESCAPE) { quit_ = true; }
                else apply_key(snap_, ev.key.keysym.sym, true);
                break;
            case SDL_KEYUP:
                apply_key(snap_, ev.key.keysym.sym, false);
                break;
        }
    }
}

void UI::render_frame(const uint8_t* px) {
    if (!texture_ || !renderer_) return;
    SDL_UpdateTexture((SDL_Texture*)texture_, nullptr, px, 256 * 4);
    SDL_RenderClear((SDL_Renderer*)renderer_);
    SDL_RenderCopy((SDL_Renderer*)renderer_, (SDL_Texture*)texture_, nullptr, nullptr);
    SDL_RenderPresent((SDL_Renderer*)renderer_);
}

void UI::push_audio(const int16_t* samples, int n_samples_stereo) {
    if (!audio_device_) return;
    // Drop if backlog is too large to keep latency bounded.
    if (SDL_GetQueuedAudioSize(audio_device_) > 8192 * 4) return;
    SDL_QueueAudio(audio_device_, samples, (Uint32)(n_samples_stereo * sizeof(int16_t)));
}

void UI::set_title(const std::string& t) {
    cfg_.title = t;
    if (window_) SDL_SetWindowTitle((SDL_Window*)window_, t.c_str());
}

void UI::set_state(EmuState s) {
    state_ = s;
    if (state_cb_) state_cb_(s);
}

void UI::load_rom(const std::string& path) {
    if (rom_cb_) rom_cb_(path);
}

void UI::set_setting(const std::string& key, const std::string& value) { settings_[key] = value; }
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

} // namespace fcemu
