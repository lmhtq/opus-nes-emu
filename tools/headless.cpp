// headless.cpp - Boot a real ROM without SDL, run N frames, dump stats + PPM snapshot.
// Used for CI-style smoke testing of real games.
#include "fcemu/cpu.h"
#include "fcemu/ppu.h"
#include "fcemu/apu.h"
#include "fcemu/memory.h"
#include "fcemu/cartridge.h"
#include "fcemu/input.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <string>

static bool load_file(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    auto sz = f.tellg(); f.seekg(0);
    out.resize((size_t)sz);
    f.read(reinterpret_cast<char*>(out.data()), sz);
    return (bool)f;
}

static void write_ppm(const std::string& path, const uint8_t* rgba, int w, int h) {
    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << w << " " << h << "\n255\n";
    for (int i = 0; i < w * h; ++i) {
        f.put(rgba[i*4+0]); f.put(rgba[i*4+1]); f.put(rgba[i*4+2]);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s rom.nes [frames=120] [snapshot.ppm]\n", argv[0]);
        return 1;
    }
    int frames_to_run = (argc >= 3) ? std::atoi(argv[2]) : 120;
    std::string snapshot = (argc >= 4) ? argv[3] : "";

    std::vector<uint8_t> rom;
    if (!load_file(argv[1], rom)) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    fcemu::Cartridge cart;
    if (!cart.load_rom_data(rom)) { std::fprintf(stderr, "invalid iNES file\n"); return 3; }
    std::printf("ROM: %s mapper=%u prg=%uKB chr=%uKB mirror=%d battery=%d sha256=%s\n",
                argv[1], cart.mapper_number(),
                (unsigned)(cart.prg_rom().size()/1024),
                (unsigned)(cart.chr().size()/1024),
                (int)cart.mirror_mode(), (int)cart.has_battery(), cart.sha256().c_str());

    fcemu::Memory mem;
    fcemu::Cpu6502 cpu;
    fcemu::Ppu ppu;
    fcemu::Apu apu; apu.init(44100);
    fcemu::InputManager input;
    input.set_controller(0, std::make_unique<fcemu::StandardController>());

    int audio_samples = 0;
    apu.set_sample_callback([&](const std::vector<int16_t>& v){ audio_samples += (int)v.size(); });
    apu.set_dmc_reader([&](uint16_t a){ return mem.read(a); });

    ppu.set_cartridge(&cart);
    ppu.set_nmi_callback([&]{ cpu.signal_nmi(); });
    mem.set_ppu_callbacks([&](uint16_t a){return ppu.cpu_read(a);}, [&](uint16_t a, uint8_t v){ppu.cpu_write(a,v);});
    mem.set_apu_callbacks([&](uint16_t a){return apu.cpu_read(a);}, [&](uint16_t a, uint8_t v){apu.cpu_write(a,v);});
    mem.set_input_callbacks([&](uint16_t a){return input.cpu_read(a);}, [&](uint16_t a, uint8_t v){input.cpu_write(a,v);});
    mem.set_cart_callbacks([&](uint16_t a){return cart.cpu_read(a);}, [&](uint16_t a, uint8_t v){cart.cpu_write(a,v);});
    mem.set_oam_dma_callback([&](uint8_t p){
        uint8_t b[256]; for(int i=0;i<256;++i) b[i]=mem.read((p<<8)|i);
        ppu.oam_dma_write(b); cpu.trigger_dma(p);
    });
    cpu.set_callbacks([&](uint16_t a){return mem.read(a);}, [&](uint16_t a, uint8_t v){mem.write(a,v);});
    cpu.reset(); ppu.reset(); apu.reset();

    long total_cycles = 0;
    int last_frame = 0;
    bool prev_irq = false;
    while (ppu.frame_count() < frames_to_run) {
        int c = cpu.step();
        ppu.step(c);
        apu.step(c);
        bool irq = cart.irq_pending();
        if (irq && !prev_irq) cpu.signal_irq();
        prev_irq = irq;
        total_cycles += c;
        if (ppu.frame_count() != last_frame) last_frame = ppu.frame_count();
    }

    const auto& fb = ppu.frame();
    int nonzero = 0;
    for (int i = 0; i < 256*240; ++i)
        if (fb.pixels[i*4+0] || fb.pixels[i*4+1] || fb.pixels[i*4+2]) ++nonzero;

    std::printf("frames=%d cycles=%ld audio_samples=%d nonzero_pixels=%d/%d\n",
                ppu.frame_count(), total_cycles, audio_samples, nonzero, 256*240);

    if (!snapshot.empty()) {
        write_ppm(snapshot, fb.pixels, 256, 240);
        std::printf("snapshot: %s\n", snapshot.c_str());
    }
    return 0;
}
