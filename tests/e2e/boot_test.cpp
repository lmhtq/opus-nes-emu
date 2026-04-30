// e2e_boot_test.cpp - Boot a synthetic NROM ROM, run N frames, verify framebuffer changes.
#include "fcemu/cpu.h"
#include "fcemu/ppu.h"
#include "fcemu/apu.h"
#include "fcemu/memory.h"
#include "fcemu/cartridge.h"
#include "fcemu/input.h"

#include <cassert>
#include <cstdio>
#include <vector>

// Build a minimal NROM that just enables PPU rendering and loops.
//   LDA #$1E   ; mask = bg + sprites + show-left8
//   STA $2001
//   LDA #$80
//   STA $2000  ; enable NMI
// loop: JMP loop
static std::vector<uint8_t> make_rom() {
    std::vector<uint8_t> rom(16 + 16384 + 8192, 0);
    rom[0]='N'; rom[1]='E'; rom[2]='S'; rom[3]=0x1A;
    rom[4]=1; rom[5]=1; rom[6]=0; rom[7]=0;
    uint8_t* prg = rom.data() + 16;
    int o = 0;
    prg[o++] = 0xA9; prg[o++] = 0x1E;       // LDA #$1E
    prg[o++] = 0x8D; prg[o++] = 0x01; prg[o++] = 0x20; // STA $2001
    prg[o++] = 0xA9; prg[o++] = 0x80;       // LDA #$80
    prg[o++] = 0x8D; prg[o++] = 0x00; prg[o++] = 0x20; // STA $2000
    prg[o++] = 0x4C; prg[o++] = 0x09; prg[o++] = 0x80; // JMP $8009 (self loop)
    // Reset vector.
    prg[0x3FFC] = 0x00; prg[0x3FFD] = 0x80;
    // Fill CHR with non-zero pattern so background renders something.
    for (int i = 0; i < 8192; ++i) rom[16 + 16384 + i] = (uint8_t)(i & 0xFF);
    return rom;
}

int main() {
    printf("=== fcemu E2E Boot Test ===\n");
    fcemu::Cartridge cart;
    bool ok = cart.load_rom_data(make_rom());
    assert(ok);

    fcemu::Memory mem;
    fcemu::Cpu6502 cpu;
    fcemu::Ppu ppu;
    fcemu::Apu apu; apu.init(44100);
    fcemu::InputManager input;
    input.set_controller(0, std::make_unique<fcemu::StandardController>());

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

    // Run ~10 frames (≈ 300k cycles).
    for (int i = 0; i < 300000; ++i) {
        int c = cpu.step();
        ppu.step(c);
        apu.step(c);
    }

    assert(ppu.frame_count() >= 5);
    // Verify framebuffer has at least some non-zero pixels.
    const auto& fb = ppu.frame();
    int nonzero = 0;
    for (int i = 0; i < 256*240; ++i) {
        if (fb.pixels[i*4+0] || fb.pixels[i*4+1] || fb.pixels[i*4+2]) ++nonzero;
    }
    printf("frames=%d nonzero_pixels=%d\n", ppu.frame_count(), nonzero);
    assert(nonzero > 0);
    printf("PASS\n");
    return 0;
}
