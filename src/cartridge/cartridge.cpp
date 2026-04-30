// cartridge.cpp - Cartridge and Mapper support (partial)
#include "fcemu/cartridge.h"
#include <cstdio>
#include <cstring>
#include <fstream>

namespace fcemu {

Cartridge::Cartridge() : mapper_number_(0), has_battery_(false),
    mirror_mode_(MirrorMode::Horizontal), irq_pending_(false) {}

Cartridge::~Cartridge() = default;

bool Cartridge::load_rom(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        printf("Failed to open ROM: %s\n", path.c_str());
        return false;
    }
    // Read entire file
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return load_rom_data(data);
}

bool Cartridge::load_rom_data(const std::vector<uint8_t>& data) {
    if (data.size() < 16) return false;
    // Check magic
    if (data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A) {
        printf("Invalid iNES header\n");
        return false;
    }
    int prg_banks = data[4];
    int chr_banks = data[5];
    int mapper_low = (data[6] >> 4) & 0x0F;
    int mapper_high = data[7] & 0xF0;
    mapper_number_ = mapper_low | mapper_high;
    has_battery_ = (data[6] & 0x02) != 0;
    // TODO: Mirror mode from flags
    // Load PRG ROM
    size_t prg_size = prg_banks * 16 * 1024;
    // TODO: Skip trainer if present (data[6] & 0x04)
    size_t offset = 16;  // Skip header
    // TODO: Create mapper
    printf("Loaded ROM: PRG=%d banks, CHR=%d banks, Mapper=%d\n",
           prg_banks, chr_banks, mapper_number_);
    return true;
}

uint8_t Cartridge::cpu_read(uint16_t addr) {
    if (mapper_) return mapper_->cpu_read(addr);
    return 0;
}

void Cartridge::cpu_write(uint16_t addr, uint8_t val) {
    if (mapper_) mapper_->cpu_write(addr, val);
}

uint8_t Cartridge::ppu_read(uint16_t addr) {
    if (mapper_) return mapper_->ppu_read(addr);
    return 0;
}

void Cartridge::ppu_write(uint16_t addr, uint8_t val) {
    if (mapper_) mapper_->ppu_write(addr, val);
}

void Cartridge::save_battery_ram(const std::string& path) {
    if (!has_battery_ || battery_ram_.empty()) return;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(battery_ram_.data()), battery_ram_.size());
}

void Cartridge::load_battery_ram(const std::string& path) {
    if (!has_battery_) return;
    std::ifstream file(path, std::ios::binary);
    if (!file) return;
    file.read(reinterpret_cast<char*>(battery_ram_.data()), battery_ram_.size());
}

void Cartridge::notify_scanline(int scanline) {
    if (mapper_) mapper_->scanline_irq(scanline);
}

} // namespace fcemu
