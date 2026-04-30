// ui.cpp - UI and Window management (stub)
#include "fcemu/ui.h"
#include <cstdio>

namespace fcemu {

UI::UI() : state_(EmuState::Stopped), window_(nullptr), gl_context_(nullptr), texture_id_(0) {}

UI::~UI() { shutdown(); }

bool UI::init(const WindowConfig& config) {
    config_ = config;
    printf("UI: Initializing window %dx%d\n", config_.width, config_.height);
    // TODO: Init SDL2 + OpenGL
    return true;
}

void UI::shutdown() {
    // TODO: Cleanup
}

void UI::run() {
    printf("UI: Entering main loop\n");
    // TODO: Main loop
}

void UI::process_events() {
    // TODO: Process SDL events
}

void UI::render_frame(const uint8_t* framebuffer) {
    // TODO: Render PPU frame to texture
}

void UI::set_render_scale(float scale) {
    config_.scale = scale;
}

void UI::load_rom(const std::string& path) {
    printf("UI: Loading ROM %s\n", path.c_str());
    if (rom_load_callback_) rom_load_callback_(path);
}

void UI::set_state(EmuState state) {
    state_ = state;
    if (state_callback_) state_callback_(state);
    printf("UI: State changed to %d\n", static_cast<int>(state));
}

bool UI::save_state(int slot) {
    printf("UI: Save state to slot %d\n", slot);
    return false;  // TODO
}

bool UI::load_state(int slot) {
    printf("UI: Load state from slot %d\n", slot);
    return false;  // TODO
}

void UI::set_setting(const std::string& key, const std::string& value) {
    // TODO: Store setting
}

std::string UI::get_setting(const std::string& key) const {
    return "";  // TODO
}

void UI::save_settings() {
    // TODO
}

void UI::load_settings() {
    // TODO
}

void UI::init_imgui() {
    // TODO: Init Dear ImGui
}

void UI::shutdown_imgui() {
    // TODO
}

void UI::render_imgui() {
    // TODO: Render ImGui overlay
}

void UI::render_main_menu() {
    // TODO
}

void UI::render_settings_window() {
    // TODO
}

} // namespace fcemu
