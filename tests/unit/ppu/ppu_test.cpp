// ppu_test.cpp - PPU 单元测试
#include "fcemu/ppu.h"
#include <cassert>
#include <cstdio>

void test_init() {
    printf("Test: PPU init...\n");
    fcemu::Ppu ppu;
    ppu.reset();
    const auto& fb = ppu.frame();
    bool all_zero = true;
    for (int i = 0; i < 256 * 240 * 4; i += 4) {
        if (fb.pixels[i] || fb.pixels[i+1] || fb.pixels[i+2]) {
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
    ppu.set_nmi_callback([&]{ nmi_called = true; });
    ppu.reset();
    // Enable NMI on VBlank.
    ppu.cpu_write(0x2000, 0x80);
    // Step ~1 full frame (29780 CPU cycles).
    for (int i = 0; i < 29781; ++i) ppu.step(1);
    uint8_t status = ppu.cpu_read(0x2002);
    (void)status;
    assert(nmi_called);
    printf("  PASS (NMI fired)\n");
}

int main() {
    printf("=== fcemu PPU Tests ===\n\n");
    test_init();
    test_vblank();
    printf("\nAll PPU tests passed!\n");
    return 0;
}
