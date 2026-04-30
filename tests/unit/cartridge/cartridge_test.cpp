// cartridge_test.cpp - iNES loader & NROM mapper smoke test.
#include "fcemu/cartridge.h"
#include <cassert>
#include <cstdio>
#include <vector>

static std::vector<uint8_t> make_nrom() {
    std::vector<uint8_t> rom;
    rom.resize(16, 0);
    rom[0]='N'; rom[1]='E'; rom[2]='S'; rom[3]=0x1A;
    rom[4] = 1; // 16KB PRG
    rom[5] = 1; // 8KB CHR
    rom[6] = 0; // mapper 0, horizontal
    rom[7] = 0;
    rom.resize(16 + 16384 + 8192, 0);
    // Fill PRG with deterministic pattern
    for (int i = 0; i < 16384; ++i) rom[16 + i] = (uint8_t)(i & 0xFF);
    // RESET vector at $FFFC: point to $8000
    rom[16 + 0x3FFC] = 0x00;
    rom[16 + 0x3FFD] = 0x80;
    return rom;
}

int main() {
    printf("=== fcemu Cartridge Tests ===\n");
    fcemu::Cartridge c;
    auto rom = make_nrom();
    bool ok = c.load_rom_data(rom);
    assert(ok);
    assert(c.mapper_number() == 0);
    assert(c.mirror_mode() == fcemu::MirrorMode::Horizontal);
    assert(!c.sha256().empty());

    // NROM with 16KB PRG mirrors $8000 == $C000.
    uint8_t a = c.cpu_read(0x8000);
    uint8_t b = c.cpu_read(0xC000);
    assert(a == b);

    // Reset vector
    uint8_t lo = c.cpu_read(0xFFFC);
    uint8_t hi = c.cpu_read(0xFFFD);
    assert(lo == 0x00 && hi == 0x80);

    printf("PASS\n");
    return 0;
}
