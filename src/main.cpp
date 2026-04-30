// main.cpp - fcemu entry point. Wires CPU+PPU+APU+Memory+Cartridge+Input+UI.
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

#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <memory>

using namespace fcemu;

namespace {

constexpr int CPU_CYCLES_PER_FRAME = 29781;

void map_buttons(StandardController& c, const InputSnapshot& s) {
    c.set_button(Button::A,      s.a);
    c.set_button(Button::B,      s.b);
    c.set_button(Button::Select, s.select);
    c.set_button(Button::Start,  s.start);
    c.set_button(Button::Up,     s.up);
    c.set_button(Button::Down,   s.down);
    c.set_button(Button::Left,   s.left);
    c.set_button(Button::Right,  s.right);
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
        cpu.trigger_dma(page); // burns CPU cycles
    });

    cpu.set_callbacks(
        [&](uint16_t a){ return mem.read(a); },
        [&](uint16_t a, uint8_t v){ mem.write(a, v); });

    UI ui;
    if (!ui.init(WindowConfig{})) {
        std::fprintf(stderr, "UI init failed\n");
        return 1;
    }
    std::string title = std::string("fcemu — ") + cart.game_name();
    ui.set_title(title);

    apu.set_sample_callback([&](const std::vector<int16_t>& s){
        ui.push_audio(s.data(), (int)s.size());
    });

    // Optional enhancers (kept but not heavily wired into the loop yet).
    VideoEnhancer venh; venh.init();
    AudioEnhancer aenh; aenh.init(44100);
    HapticsManager haptics; haptics.init();
    ReplayManager replay; replay.init(); replay.start_recording();
    PresetManager presets; presets.init("presets");
    presets.find_matching_preset(cart.sha256(), PresetType::All);

    cpu.reset();
    ppu.reset();
    apu.reset();

    Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 frame_target_ticks = perf_freq / 60;
    Uint64 next_tick = SDL_GetPerformanceCounter() + frame_target_ticks;

    while (!ui.should_quit()) {
        ui.process_events();
        auto snap = ui.input_snapshot();
        if (snap.reset) { cpu.reset(); ppu.reset(); apu.reset(); }
        if (auto* sc = dynamic_cast<StandardController*>(input.get_controller(0))) {
            map_buttons(*sc, snap);
        }

        // Run one frame. Drive components in lockstep on CPU cycles.
        int budget = CPU_CYCLES_PER_FRAME;
        while (budget > 0) {
            if (cart.irq_pending()) cpu.signal_irq();
            int c = cpu.step();
            ppu.step(c);
            apu.step(c);
            budget -= c;
            if (ppu.frame_complete()) break;
        }

        ui.render_frame(ppu.frame().pixels);

        Uint64 now = SDL_GetPerformanceCounter();
        if (now < next_tick) {
            Uint32 ms = (Uint32)((next_tick - now) * 1000 / perf_freq);
            if (ms > 0 && ms < 100) SDL_Delay(ms);
        }
        next_tick += frame_target_ticks;
    }

    ui.shutdown();
    return 0;
}
