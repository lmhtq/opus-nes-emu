// memory.cpp - Memory mapping (partial implementation)
#include "fcemu/memory.h"
#include <cstring>

namespace fcemu {

Memory::Memory() : has_battery_ram_(false), mapper_read_(nullptr),
    mapper_write_(nullptr), mapper_range_start_(0), mapper_range_end_(0),
    ppu_read_(nullptr), ppu_write_(nullptr),
    apu_read_(nullptr), apu_write_(nullptr) {
    std::memset(ram_.data(), 0, ram_.size());
}

void Memory::reset() {
    std::memset(ram_.data(), 0, ram_.size());
    prg_rom_.clear();
    battery_ram_.clear();
    has_battery_ram_ = false;
}

uint8_t Memory::read(uint16_t addr) {
    // Internal RAM + mirrors
    if (addr < 0x2000) {
        return ram_[mirror_ram_addr(addr)];
    }
    // PPU registers + mirrors
    if (addr < 0x4000) {
        if (ppu_read_) return ppu_read_(addr);
        return 0;
    }
    // APU / IO
    if (addr < 0x4020) {
        if (apu_read_) return apu_read_(addr);
        return 0;
    }
    // Cartridge space
    if (mapper_read_) return mapper_read_(addr);
    return 0;
}

void Memory::write(uint16_t addr, uint8_t val) {
    if (addr < 0x2000) {
        ram_[mirror_ram_addr(addr)] = val;
        return;
    }
    if (addr < 0x4000) {
        if (ppu_write_) ppu_write_(addr, val);
        return;
    }
    if (addr < 0x4020) {
        if (apu_write_) apu_write_(addr, val);
        return;
    }
    if (mapper_write_) mapper_write_(addr, val);
}

void Memory::register_mapper_read(uint16_t start, uint16_t end, MemReadCallback cb) {
    mapper_read_ = cb;
    mapper_range_start_ = start;
    mapper_range_end_ = end;
}

void Memory::register_mapper_write(uint16_t start, uint16_t end, MemWriteCallback cb) {
    mapper_write_ = cb;
}

void Memory::set_ppu_callbacks(MemReadCallback read, MemWriteCallback write) {
    ppu_read_ = read;
    ppu_write_ = write;
}

void Memory::set_apu_callbacks(MemReadCallback read, MemWriteCallback write) {
    apu_read_ = read;
    apu_write_ = write;
}

void Memory::load_prg_rom(const std::vector<uint8_t>& data) {
    prg_rom_ = data;
}

void Memory::set_prg_rom_bank(int slot, size_t bank) {
    // TODO: implement bank switching for mappers
}

void Memory::set_battery_ram_size(size_t size) {
    battery_ram_.resize(size);
    has_battery_ram_ = (size > 0);
}

uint16_t Memory::mirror_ram_addr(uint16_t addr) const {
    return addr & 0x07FF;  // Mirror to 0x0000-0x07FF
}

uint16_t Memory::mirror_ppu_addr(uint16_t addr) const {
    return addr & 0x2007;  // Mirror to 0x2000-0x2007
}

} // namespace fcemu
