// main.cpp - fcemu entry point. Wires CPU+PPU+APU+Memory+Cartridge+Input+UI
// and the optional advanced experience modules (video / audio / haptics /
// replay / social).
#include "fcemu/cpu.h"
#include "fcemu/ppu.h"
#include "fcemu/apu.h"
#include "fcemu/memory.h"
#include "fcemu/cartridge.h"
#include "fcemu/input.h"
#include "fcemu/ui.h"
#include "fcemu/video_enhancer.h"
#include "fcemu/audio_enhancer.h"
#include "fcemu/haptics.h"
#include "fcemu/replay.h"
#include "fcemu/presets.h"
#include "fcemu/social.h"
#include "fcemu/savestate.h"
#include "fcemu/menu.h"

#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace fcemu;

namespace {

constexpr int CPU_CYCLES_PER_FRAME = 29781;
constexpr uint32_t SAVESTATE_MAGIC   = 0x46434553u; // 'FCES'
constexpr uint32_t SAVESTATE_VERSION = 1;

void map_buttons(StandardController& c, const InputSnapshot& s, bool p2) {
    if (!p2) {
        c.set_button(Button::A,      s.a);
        c.set_button(Button::B,      s.b);
        c.set_button(Button::Select, s.select);
        c.set_button(Button::Start,  s.start);
        c.set_button(Button::Up,     s.up);
        c.set_button(Button::Down,   s.down);
        c.set_button(Button::Left,   s.left);
        c.set_button(Button::Right,  s.right);
        c.set_turbo (Button::A, s.a_turbo);
        c.set_turbo (Button::B, s.b_turbo);
    } else {
        c.set_button(Button::A,      s.p2_a);
        c.set_button(Button::B,      s.p2_b);
        c.set_button(Button::Select, s.p2_select);
        c.set_button(Button::Start,  s.p2_start);
        c.set_button(Button::Up,     s.p2_up);
        c.set_button(Button::Down,   s.p2_down);
        c.set_button(Button::Left,   s.p2_left);
        c.set_button(Button::Right,  s.p2_right);
        c.set_turbo (Button::A, s.p2_a_turbo);
        c.set_turbo (Button::B, s.p2_b_turbo);
    }
}

bool save_state_to_file(const std::string& path,
                        const Cpu6502& cpu, const Memory& mem,
                        const Ppu& ppu, const Apu& apu, const Cartridge& cart) {
    std::vector<uint8_t> blob;
    Serializer s(blob);
    s.write(SAVESTATE_MAGIC);
    s.write(SAVESTATE_VERSION);
    int mapper_no = cart.mapper_number();
    s.write(mapper_no);
    cpu.serialize(s);
    mem.serialize(s);
    ppu.serialize(s);
    apu.serialize(s);
    cart.serialize(s);
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(blob.data()), blob.size());
    return (bool)f;
}

bool load_state_from_file(const std::string& path,
                          Cpu6502& cpu, Memory& mem,
                          Ppu& ppu, Apu& apu, Cartridge& cart) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    size_t sz = (size_t)f.tellg();
    f.seekg(0);
    std::vector<uint8_t> blob(sz);
    f.read(reinterpret_cast<char*>(blob.data()), sz);
    if (!f) return false;
    try {
        Deserializer d(blob.data(), blob.size());
        uint32_t magic, ver; int mapper_no;
        d.read(magic); d.read(ver); d.read(mapper_no);
        if (magic != SAVESTATE_MAGIC || ver != SAVESTATE_VERSION) return false;
        if (mapper_no != cart.mapper_number()) return false;
        cpu.deserialize(d);
        mem.deserialize(d);
        ppu.deserialize(d);
        apu.deserialize(d);
        cart.deserialize(d);
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "load_state: %s\n", e.what());
        return false;
    }
}

// Heuristic audio scene from APU rms.
const char* infer_scene(const std::vector<int16_t>& samples) {
    if (samples.empty()) return "calm";
    int64_t sumsq = 0;
    for (int16_t s : samples) sumsq += (int64_t)s * s;
    double rms = std::sqrt((double)sumsq / samples.size());
    if      (rms > 9000) return "boss";
    else if (rms > 5000) return "action";
    else if (rms > 1500) return "menu";
    else                 return "calm";
}

} // namespace

int main(int argc, char* argv[]) {
    std::printf("fcemu - FC/NES Emulator with Modern Experience\n");
    std::printf("Version 0.1.0\n");

    if (argc < 2) {
        std::printf("Usage: %s <rom_file.nes>\n", argv[0]);
        return 1;
    }

    Cartridge cart;
    if (!cart.load_rom(argv[1])) {
        std::fprintf(stderr, "Failed to load ROM: %s\n", argv[1]);
        return 1;
    }
    std::printf("ROM: %s\n  mapper=%d  battery=%d  sha256=%s\n",
                cart.game_name().c_str(), cart.mapper_number(),
                cart.has_battery(), cart.sha256().c_str());

    Memory mem;
    Cpu6502 cpu;
    Ppu ppu;
    Apu apu;
    InputManager input;
    input.set_controller(0, std::make_unique<StandardController>());
    input.set_controller(1, std::make_unique<StandardController>());

    ppu.set_cartridge(&cart);
    ppu.set_nmi_callback([&]{ cpu.signal_nmi(); });
    apu.init(44100);
    apu.set_dmc_reader([&](uint16_t a){ return mem.read(a); });

    mem.set_ppu_callbacks(
        [&](uint16_t a){ return ppu.cpu_read(a); },
        [&](uint16_t a, uint8_t v){ ppu.cpu_write(a, v); });
    mem.set_apu_callbacks(
        [&](uint16_t a){ return apu.cpu_read(a); },
        [&](uint16_t a, uint8_t v){ apu.cpu_write(a, v); });
    mem.set_input_callbacks(
        [&](uint16_t a){ return input.cpu_read(a); },
        [&](uint16_t a, uint8_t v){ input.cpu_write(a, v); });
    mem.set_cart_callbacks(
        [&](uint16_t a){ return cart.cpu_read(a); },
        [&](uint16_t a, uint8_t v){ cart.cpu_write(a, v); });
    mem.set_oam_dma_callback([&](uint8_t page){
        uint8_t buf[256];
        for (int i = 0; i < 256; ++i) buf[i] = mem.read((page << 8) | i);
        ppu.oam_dma_write(buf);
        cpu.trigger_dma(page);
    });

    cpu.set_callbacks(
        [&](uint16_t a){ return mem.read(a); },
        [&](uint16_t a, uint8_t v){ mem.write(a, v); });

    UI ui;
    if (!ui.init(WindowConfig{})) {
        std::fprintf(stderr, "UI init failed\n");
        return 1;
    }
    ui.load_settings("fcemu.ini");
    ui.seed_default_bindings();
    ui.set_hud_visible(ui.get_setting("ui.hud") == "true");
    {
        auto cs = ui.find_key_conflicts();
        if (!cs.empty()) {
            std::printf("[ui] WARNING: %zu key binding conflict(s) detected:\n", cs.size());
            for (auto& p : cs) std::printf("  %s = %s\n", p.first.c_str(), p.second.c_str());
            std::printf("[ui]   open Menu -> Controls -> Scan for key conflicts to review.\n");
        }
    }
    {
        auto rate_str = ui.get_setting("turbo.rate_frames");
        if (!rate_str.empty()) {
            int r = std::atoi(rate_str.c_str());
            for (int p = 0; p < 2; ++p) {
                if (auto* sc = dynamic_cast<StandardController*>(input.get_controller(p)))
                    sc->set_turbo_rate(r > 0 ? r : 2);
            }
        }
    }
    ui.set_title(std::string("fcemu — ") + cart.game_name());

    // ---- Enhancement modules --------------------------------------------
    VideoEnhancer venh; venh.init();
    venh.enable_widescreen(ui.get_setting("video.widescreen") == "true");
    {
        CRTEffect crt{};
        crt.enabled = ui.get_setting("video.crt") != "false";
        crt.scanline_intensity = 0.35f;
        venh.set_crt_params(crt);
    }
    AudioEnhancer aenh; aenh.init(44100);
    HapticsManager haptics; haptics.init();
    haptics.set_rgb_callback([](RGBColor c){
        // Hardware integration is platform-specific; print only when
        // something downstream actually consumes this.
        (void)c;
    });
    ReplayManager replay; replay.init(); replay.start_recording();
    PresetManager presets; presets.init("presets");
    presets.find_matching_preset(cart.sha256(), PresetType::All);

    SocialBridge social;
    std::string watch_path = ui.get_setting("social.watch_file");
    social.init(watch_path);
    social.set_handler([&](const SocialEvent& ev){
        switch (ev.type) {
            case SocialEventType::Cheer:
                venh.trigger_hit_flash();
                haptics.trigger_vibration(VibrationIntensity::Medium, ev.duration_ms);
                ui.overlay().post_toast("Cheer!", 1.5f, Overlay::Yellow);
                break;
            case SocialEventType::Shake:
                venh.trigger_shake(ev.intensity / 100.0f, ev.duration_ms);
                haptics.trigger_vibration(VibrationIntensity::Strong, ev.duration_ms);
                ui.overlay().post_toast("Shake!", 1.2f, Overlay::Red);
                break;
            case SocialEventType::Gift:
                if (auto* sc = dynamic_cast<StandardController*>(input.get_controller(0))) {
                    sc->set_turbo(Button::A, true);
                }
                haptics.trigger_vibration(VibrationIntensity::Strong, ev.duration_ms);
                {
                    std::ostringstream os; os << "Gift " << ev.kind << " x" << ev.count;
                    ui.overlay().post_toast(os.str(), 2.0f, Overlay::Green);
                }
                break;
            case SocialEventType::Vote:
                ui.overlay().post_toast(std::string("Vote ") + ev.text, 1.5f, Overlay::Cyan);
                break;
            case SocialEventType::Chat:
                ui.overlay().post_toast(std::string("Chat: ") + ev.text, 2.5f, Overlay::White);
                break;
            default: break;
        }
    });

    // ---- Build the in-game menu (ESC / F4 to open) ----------------------
    auto root = std::make_shared<Menu>("fcemu");

    // Settings → Video
    auto m_video = std::make_shared<Menu>("Video");
    m_video->add(MenuItem::toggle("CRT scanlines",
        [&]{ return ui.get_setting("video.crt") != "false"; },
        [&](bool v){
            ui.set_setting("video.crt", v ? "true" : "false");
            CRTEffect c{}; c.enabled = v; c.scanline_intensity = 0.35f;
            venh.set_crt_params(c);
            ui.overlay().post_toast(v ? "CRT ON" : "CRT OFF", 1.0f);
        }));
    m_video->add(MenuItem::toggle("Widescreen 320x240",
        [&]{ return venh.widescreen_enabled(); },
        [&](bool v){
            venh.enable_widescreen(v);
            ui.set_setting("video.widescreen", v ? "true" : "false");
            ui.overlay().post_toast(v ? "Widescreen ON" : "Widescreen OFF", 1.0f);
        }));

    // Settings → Audio
    auto m_audio = std::make_shared<Menu>("Audio");
    static const std::vector<std::string> scene_opts =
        {"auto", "calm", "menu", "action", "boss", "victory"};
    m_audio->add(MenuItem::choice("Scene", scene_opts,
        [&]{
            std::string s = ui.get_setting("audio.scene");
            if (s.empty()) s = "auto";
            for (size_t i = 0; i < scene_opts.size(); ++i)
                if (scene_opts[i] == s) return (int)i;
            return 0;
        },
        [&](int v){
            std::string s = scene_opts[v];
            ui.set_setting("audio.scene", s);
            if (s != "auto") aenh.set_scene(s);
            ui.overlay().post_toast("Scene: " + s, 1.0f, Overlay::Cyan);
        }));

    // Settings → Haptics
    auto m_haptics = std::make_shared<Menu>("Haptics");
    m_haptics->add(MenuItem::action("Test rumble (strong 400ms)", [&]{
        haptics.trigger_vibration(VibrationIntensity::Strong, 400);
        ui.overlay().post_toast("Rumble!", 1.0f, Overlay::Red);
    }));

    // Controls → P1 / P2 keybinds
    auto build_keybind_menu = [&](const std::string& title, const std::string& prefix){
        auto m = std::make_shared<Menu>(title);
        for (auto& act : UI::action_ids()) {
            if (act.rfind(prefix, 0) != 0) continue;
            std::string label = act.substr(prefix.size());
            m->add(MenuItem::keybind(label, act,
                [&, act]{
                    auto v = ui.get_setting("key." + act);
                    return v.empty() ? std::string("?") : v;
                },
                [&, act](const std::string& name){
                    // Detect & auto-resolve conflicts: if another action already
                    // owns this key, unbind the other one and warn the user.
                    std::string other = ui.find_action_for_key(name, act);
                    if (!other.empty()) {
                        ui.clear_setting("key." + other);
                        ui.overlay().post_toast(
                            other + " unbound (was " + name + ")",
                            2.0f, Overlay::Red);
                    }
                    ui.set_setting("key." + act, name);
                    ui.overlay().post_toast(act + " -> " + name, 1.2f, Overlay::Yellow);
                }));
        }
        return m;
    };
    auto m_p1 = build_keybind_menu("Player 1 keys", "p1.");
    auto m_p2 = build_keybind_menu("Player 2 keys", "p2.");

    auto m_controls = std::make_shared<Menu>("Controls");
    m_controls->add(MenuItem::submenu("Player 1 keys", m_p1));
    m_controls->add(MenuItem::submenu("Player 2 keys", m_p2));
    m_controls->add(MenuItem::action("Reset all keys to default", [&]{
        ui.reset_default_bindings();
        ui.overlay().post_toast("All key bindings reset to defaults", 2.0f, Overlay::Yellow);
    }));
    m_controls->add(MenuItem::action("Scan for key conflicts", [&]{
        auto cs = ui.find_key_conflicts();
        if (cs.empty()) {
            ui.overlay().post_toast("No key conflicts.", 1.5f, Overlay::Green);
        } else {
            std::ostringstream os;
            os << cs.size() << " conflict(s): ";
            for (size_t i = 0; i < cs.size() && i < 4; ++i) {
                if (i) os << ", ";
                os << cs[i].first << "=" << cs[i].second;
            }
            ui.overlay().post_toast(os.str(), 3.0f, Overlay::Red);
        }
    }));

    // Settings root
    auto m_settings = std::make_shared<Menu>("Settings");
    m_settings->add(MenuItem::submenu("Video",   m_video));
    m_settings->add(MenuItem::submenu("Audio",   m_audio));
    m_settings->add(MenuItem::submenu("Haptics", m_haptics));
    m_settings->add(MenuItem::toggle("HUD overlay (Tab)",
        [&]{ return ui.hud_visible(); },
        [&](bool v){ ui.set_hud_visible(v); ui.set_setting("ui.hud", v ? "true" : "false"); }));

    // Root
    bool want_save = false, want_load = false, want_reset = false;
    root->add(MenuItem::action("Resume",      [&]{ ui.menu().close(); }));
    root->add(MenuItem::action("Save state (F1)", [&]{ want_save  = true; ui.menu().close(); }));
    root->add(MenuItem::action("Load state (F2)", [&]{ want_load  = true; ui.menu().close(); }));
    root->add(MenuItem::action("Reset (F5)",  [&]{ want_reset = true; ui.menu().close(); }));
    root->add(MenuItem::submenu("Settings",   m_settings));
    root->add(MenuItem::submenu("Controls",   m_controls));
    root->add(MenuItem::action("Quit",        [&]{ ui.menu().close(); /* set quit below */
        SDL_Event q; q.type = SDL_QUIT; SDL_PushEvent(&q);
    }));
    ui.set_root_menu(root);


    // Audio path: APU -> AudioEnhancer -> SDL queue.
    apu.set_sample_callback([&](const std::vector<int16_t>& s){
        std::vector<int16_t> processed;
        aenh.process_samples(s, processed);
        ui.push_audio(processed.data(), (int)processed.size());

        // Auto-scene heuristic — only when user hasn't pinned a scene.
        static int scene_div = 0;
        if (++scene_div >= 30) {
            scene_div = 0;
            std::string forced = ui.get_setting("audio.scene");
            if (forced.empty() || forced == "auto") {
                const char* scene = infer_scene(s);
                if (aenh.current_scene() != scene) {
                    aenh.set_scene(scene);
                    ui.overlay().post_toast(std::string("Scene: ") + scene, 1.0f, Overlay::Cyan);
                }
            }
        }
    });

    cpu.reset();
    ppu.reset();
    apu.reset();

    Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 frame_target_ticks = perf_freq / 60;
    Uint64 next_tick = SDL_GetPerformanceCounter() + frame_target_ticks;

    std::string state_path = std::string(argv[1]) + ".state";
    int  frame_no = 0;
    Uint64 last_stat = SDL_GetPerformanceCounter();

    while (!ui.should_quit()) {
        ui.process_events();
        auto snap = ui.input_snapshot();

        // Menu actions can request these too via flags set in the menu lambdas.
        bool do_save  = snap.save_state || want_save;
        bool do_load  = snap.load_state || want_load;
        bool do_reset = snap.reset      || want_reset;
        want_save = want_load = want_reset = false;

        if (do_reset) {
            cpu.reset(); ppu.reset(); apu.reset();
            ui.overlay().post_toast("Reset", 1.0f, Overlay::Yellow);
        }
        if (do_save) {
            if (save_state_to_file(state_path, cpu, mem, ppu, apu, cart))
                ui.overlay().post_toast("Saved", 1.2f, Overlay::Green);
            else
                ui.overlay().post_toast("Save FAILED", 2.0f, Overlay::Red);
        }
        if (do_load) {
            if (load_state_from_file(state_path, cpu, mem, ppu, apu, cart))
                ui.overlay().post_toast("Loaded", 1.2f, Overlay::Green);
            else
                ui.overlay().post_toast("Load FAILED", 2.0f, Overlay::Red);
        }

        auto* sc1 = dynamic_cast<StandardController*>(input.get_controller(0));
        auto* sc2 = dynamic_cast<StandardController*>(input.get_controller(1));
        if (sc1) { map_buttons(*sc1, snap, false); sc1->tick_turbo(); }
        if (sc2) { map_buttons(*sc2, snap, true);  sc2->tick_turbo(); }

        social.tick();

        // Pause emulation while menu is up.
        if (!ui.menu_open()) {
            int budget = CPU_CYCLES_PER_FRAME;
            bool prev_irq = false;
            while (budget > 0) {
                bool now_irq = cart.irq_pending();
                if (now_irq && !prev_irq) cpu.signal_irq();
                prev_irq = now_irq;

                int c = cpu.step();
                ppu.step(c);
                apu.step(c);
                budget -= c;
                if (ppu.frame_complete()) break;
            }
        }

        // Post-process video and present (menu/HUD composited inside ui).
        int ow = 256, oh = 240;
        const uint8_t* out = venh.process(ppu.frame().pixels, &ow, &oh);
        if (!out) { out = ppu.frame().pixels; ow = 256; oh = 240; }

        // Refresh HUD lines.
        if (ui.hud_visible()) {
            std::vector<std::string> lines;
            lines.push_back(std::string("Scene ") + aenh.current_scene());
            lines.push_back(std::string("WS    ") + (venh.widescreen_enabled() ? "on" : "off"));
            char rgb[24];
            std::snprintf(rgb, sizeof(rgb), "RGB   %02X%02X%02X",
                          haptics.current_color().r,
                          haptics.current_color().g,
                          haptics.current_color().b);
            lines.push_back(rgb);
            if (ui.menu_open()) lines.push_back("PAUSED");
            ui.set_hud_lines(std::move(lines));
        }

        ui.render_frame(out, ow, oh);
        haptics.update_from_frame(ppu.frame().pixels);

        if (replay.recording()) {
            FrameData fd{};
            fd.video.assign(ppu.frame().pixels, ppu.frame().pixels + 256*240*4);
            fd.timestamp = (uint64_t)frame_no * 16;
            (void)fd;
        }

        ++frame_no;
        if (ui.debug_overlay()) {
            Uint64 now = SDL_GetPerformanceCounter();
            if (now - last_stat > perf_freq) {
                std::printf("[debug] frame=%d fps=%.1f scene=%s widescreen=%d rgb=#%02X%02X%02X\n",
                            frame_no, ui.fps(),
                            aenh.current_scene().c_str(),
                            (int)venh.widescreen_enabled(),
                            haptics.current_color().r,
                            haptics.current_color().g,
                            haptics.current_color().b);
                last_stat = now;
            }
        }

        Uint64 now = SDL_GetPerformanceCounter();
        if (now < next_tick) {
            Uint32 ms = (Uint32)((next_tick - now) * 1000 / perf_freq);
            if (ms > 0 && ms < 100) SDL_Delay(ms);
        }
        next_tick += frame_target_ticks;
    }

    // Persist battery RAM if the cartridge has it.
    if (cart.has_battery()) cart.save_battery_ram(std::string(argv[1]) + ".sav");

    ui.save_settings("fcemu.ini");
    ui.shutdown();
    return 0;
}
