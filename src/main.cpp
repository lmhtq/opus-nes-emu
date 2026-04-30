// main.cpp - fcemu entry point
#include <cstdio>
#include <cstdlib>
#include <memory>
#include "fcemu/cpu.h"
#include "fcemu/ppu.h"
#include "fcemu/apu.h"
#include "fcemu/memory.h"
#include "fcemu/cartridge.h"
#include "fcemu/input.h"
#include "fcemu/ui.h"

int main(int argc, char* argv[]) {
    printf("fcemu - FC/NES Emulator with Modern Experience\n");
    printf("Version 0.1.0\n");

    if (argc < 2) {
        printf("Usage: %s <rom_file.nes>\n", argv[0]);
        return 1;
    }

    // 初始化系统
    fcemu::Cartridge cart;
    if (!cart.load_rom(argv[1])) {
        printf("Failed to load ROM: %s\n", argv[1]);
        return 1;
    }
    printf("Loaded ROM: %s\n", cart.game_name().c_str());
    printf("Mapper: %d\n", cart.mapper_number());

    // TODO: Initialize full system
    // - Memory with cartridge mapping
    // - CPU with memory callbacks
    // - PPU with CPU callbacks
    // - APU
    // - Input
    // - UI (SDL2 + OpenGL)
    // - Enhancement modules

    printf("fcemu initialized. Full implementation in progress...\n");
    return 0;
}
