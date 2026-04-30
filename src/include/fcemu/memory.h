// include/fcemu/memory.h
#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <array>

namespace fcemu {

using MemReadCallback = std::function<uint8_t(uint16_t)>;
using MemWriteCallback = std::function<void(uint16_t, uint8_t)>;

class Memory {
public:
    Memory();
    void reset();
    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t val);
    uint8_t* internal_ram() { return ram_.data(); }
    const uint8_t* internal_ram() const { return ram_.data(); }
    void register_mapper_read(uint16_t start, uint16_t end, MemReadCallback cb);
    void register_mapper_write(uint16_t start, uint16_t end, MemWriteCallback cb);
    void set_ppu_callbacks(MemReadCallback read, MemWriteCallback write);
    void set_apu_callbacks(MemReadCallback read, MemWriteCallback write);
    void load_prg_rom(const std::vector<uint8_t>& data);
    void set_prg_rom_bank(int slot, size_t bank);
    bool has_battery_ram() const { return has_battery_ram_; }
    uint8_t* battery_ram() { return battery_ram_.data(); }
    void set_battery_ram_size(size_t size);

private:
    std::array<uint8_t, 0x0800> ram_;
    std::vector<uint8_t> prg_rom_;
    std::vector<uint8_t> battery_ram_;
    bool has_battery_ram_;
    MemReadCallback mapper_read_;
    MemWriteCallback mapper_write_;
    uint16_t mapper_range_start_, mapper_range_end_;
    MemReadCallback ppu_read_;
    MemWriteCallback ppu_write_;
    MemReadCallback apu_read_;
    MemWriteCallback apu_write_;

    uint16_t mirror_ram_addr(uint16_t addr) const;
    uint16_t mirror_ppu_addr(uint16_t addr) const;
};

} // namespace fcemu
