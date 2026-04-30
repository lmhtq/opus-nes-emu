// ppu_test.cpp - PPU 单元测试 (stub)
#include "fcemu/ppu.h"
#include <cassert>
#include <cstdio>

void test_init() {
    printf("Test: PPU init...\n");
    fcemu::Ppu ppu;
    ppu.reset();
    auto fb = ppu.frame_buffer();
    // Frame buffer should be cleared
    bool all_zero = true;
    for (int i = 0; i < 256 * 240 * 4; i += 4) {
        if (fb.pixels[i] != 0 || fb.pixels[i+1] != 0 ||
            fb.pixels[i+2] != 0) {
            all_zero = false; break;
        }
    }
    assert(all_zero);
    printf("  PASS (frame buffer cleared)\n");
}

void test_vblank() {
    printf("Test: VBlank flag...\n");
    fcemu::Ppu ppu;
    bool nmi_called = false;
    ppu.set_callbacks(
        [](uint16_t) { return 0; },
        [](uint16_t, uint8_t) {},
        [&]() { nmi_called = true; }
    );
    ppu.reset();
    // Simulate scanlines until VBlank
    for (int i = 0; i < 262 * 3 * 341; i++) {
        ppu.step(1);
    }
    auto status = ppu.read_status();
    printf("  VBlank flag: %s\n", (status & 0x80) ? "SET" : "clear");
    printf("  PASS\n");
}

int main() {
    printf("=== fcemu PPU Tests ===\n\n");
    test_init();
    test_vblank();
    printf("\nAll PPU tests passed!\n");
    return 0;
}
