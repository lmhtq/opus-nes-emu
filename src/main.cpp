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

#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
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
                break;
            case SocialEventType::Shake:
                venh.trigger_shake(ev.intensity / 100.0f, ev.duration_ms);
                haptics.trigger_vibration(VibrationIntensity::Strong, ev.duration_ms);
                break;
            case SocialEventType::Gift:
                if (auto* sc = dynamic_cast<StandardController*>(input.get_controller(0))) {
                    // Burst-press A as a "gift bonus" turbo for a short window.
                    sc->set_turbo(Button::A, true);
                }
                haptics.trigger_vibration(VibrationIntensity::Strong, ev.duration_ms);
                std::printf("[social] gift %s x%d\n", ev.kind.c_str(), ev.count);
                break;
            case SocialEventType::Vote:
                std::printf("[social] vote %s\n", ev.text.c_str());
                break;
            case SocialEventType::Chat:
                std::printf("[social] chat: %s\n", ev.text.c_str());
                break;
            default: break;
        }
    });

    // Audio path: APU -> AudioEnhancer -> SDL queue.
    apu.set_sample_callback([&](const std::vector<int16_t>& s){
        std::vector<int16_t> processed;
        aenh.process_samples(s, processed);
        ui.push_audio(processed.data(), (int)processed.size());

        // Feed the replay buffer (latest video filled later in the frame).
        // Update audio scene heuristic occasionally.
        static int scene_div = 0;
        if (++scene_div >= 30) {
            scene_div = 0;
            const char* scene = infer_scene(s);
            if (aenh.current_scene() != scene) aenh.set_scene(scene);
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
        if (snap.reset) { cpu.reset(); ppu.reset(); apu.reset(); }
        if (snap.save_state) {
            if (save_state_to_file(state_path, cpu, mem, ppu, apu, cart))
                std::printf("[state] saved -> %s\n", state_path.c_str());
            else
                std::printf("[state] save FAILED\n");
        }
        if (snap.load_state) {
            if (load_state_from_file(state_path, cpu, mem, ppu, apu, cart))
                std::printf("[state] loaded <- %s\n", state_path.c_str());
            else
                std::printf("[state] load FAILED (no/incompatible state)\n");
        }

        auto* sc1 = dynamic_cast<StandardController*>(input.get_controller(0));
        auto* sc2 = dynamic_cast<StandardController*>(input.get_controller(1));
        if (sc1) { map_buttons(*sc1, snap, false); sc1->tick_turbo(); }
        if (sc2) { map_buttons(*sc2, snap, true);  sc2->tick_turbo(); }

        social.tick();

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

        // Post-process video and present.
        int ow = 256, oh = 240;
        const uint8_t* out = venh.process(ppu.frame().pixels, &ow, &oh);
        if (!out) { out = ppu.frame().pixels; ow = 256; oh = 240; }
        ui.render_frame(out, ow, oh);
        haptics.update_from_frame(ppu.frame().pixels);

        if (replay.recording()) {
            FrameData fd{};
            fd.video.assign(ppu.frame().pixels, ppu.frame().pixels + 256*240*4);
            fd.timestamp = (uint64_t)frame_no * 16;
            // Pass-through APU samples are already consumed by the SDL queue;
            // skip storing audio to keep memory usage bounded.
            // (Replay clip generator still produces a still PPM.)
            (void)fd;
        }

        ++frame_no;
        if (ui.debug_overlay()) {
            Uint64 now = SDL_GetPerformanceCounter();
            if (now - last_stat > perf_freq) {
                double secs = (now - last_stat) / (double)perf_freq;
                std::printf("[debug] frame=%d fps≈%.1f scene=%s widescreen=%d rgb=#%02X%02X%02X\n",
                            frame_no, 1.0 / (secs / 60.0),
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
