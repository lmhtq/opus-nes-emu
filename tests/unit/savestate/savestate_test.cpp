// savestate_test.cpp - round-trip CPU+PPU+APU+Memory+Cartridge save/load.
#include "fcemu/cpu.h"
#include "fcemu/memory.h"
#include "fcemu/ppu.h"
#include "fcemu/apu.h"
#include "fcemu/cartridge.h"
#include "fcemu/savestate.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

static std::vector<uint8_t> make_nrom() {
    std::vector<uint8_t> rom(16 + 16384 + 8192, 0);
    rom[0]='N'; rom[1]='E'; rom[2]='S'; rom[3]=0x1A;
    rom[4]=1; rom[5]=1; rom[6]=0; rom[7]=0;
    for (int i = 0; i < 16384; ++i) rom[16+i] = (uint8_t)(i & 0xFF);
    // Reset vector $FFFC -> $8000.
    rom[16 + 0x3FFC] = 0x00;
    rom[16 + 0x3FFD] = 0x80;
    return rom;
}

int main() {
    using namespace fcemu;
    std::printf("=== fcemu Save State Tests ===\n");

    Cartridge cart; assert(cart.load_rom_data(make_nrom()));
    Cpu6502 cpu; Memory mem; Ppu ppu; Apu apu;
    apu.init(44100);
    ppu.set_cartridge(&cart);
    cpu.set_callbacks([&](uint16_t a){ return mem.read(a); },
                      [&](uint16_t a, uint8_t v){ mem.write(a,v); });
    mem.set_cart_callbacks([&](uint16_t a){ return cart.cpu_read(a); },
                           [&](uint16_t a, uint8_t v){ cart.cpu_write(a,v); });

    cpu.reset(); ppu.reset(); apu.reset();

    // Mutate state: set a register, write some RAM, advance ppu cycle.
    Registers r = cpu.registers();
    r.a = 0xAB; r.x = 0x12; r.y = 0x34; r.pc = 0xC0DE; r.sp = 0xF0;
    cpu.set_registers(r);
    mem.internal_ram()[0x100] = 0x77;
    mem.internal_ram()[0x500] = 0x99;
    ppu.step(50);

    // Save.
    std::vector<uint8_t> blob;
    Serializer s(blob);
    cpu.serialize(s); mem.serialize(s); ppu.serialize(s);
    apu.serialize(s); cart.serialize(s);
    assert(!blob.empty());

    // Mutate everything.
    cpu.set_registers({});
    std::memset(mem.internal_ram(), 0, 0x800);
    ppu.reset();

    Cpu6502 cpu_check;
    cpu_check.set_callbacks([&](uint16_t a){ return mem.read(a); },
                            [&](uint16_t a, uint8_t v){ mem.write(a,v); });

    // Restore.
    Deserializer d(blob.data(), blob.size());
    cpu.deserialize(d); mem.deserialize(d); ppu.deserialize(d);
    apu.deserialize(d); cart.deserialize(d);

    auto rr = cpu.registers();
    assert(rr.a == 0xAB);
    assert(rr.x == 0x12);
    assert(rr.y == 0x34);
    assert(rr.pc == 0xC0DE);
    assert(rr.sp == 0xF0);
    assert(mem.internal_ram()[0x100] == 0x77);
    assert(mem.internal_ram()[0x500] == 0x99);

    std::printf("PASS\n");
    return 0;
}
