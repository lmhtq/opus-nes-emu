// include/fcemu/memory.h
#pragma once

#include <cstdint>
#include <functional>
#include <array>

namespace fcemu {

using MemReadCallback  = std::function<uint8_t(uint16_t)>;
using MemWriteCallback = std::function<void(uint16_t, uint8_t)>;
using OamDmaCallback   = std::function<void(uint8_t page)>;

// CPU memory bus.
//
// Layout:
//   $0000-$1FFF  2KB internal RAM (mirrored every 0x800)
//   $2000-$3FFF  PPU registers (mirrored every 8 bytes)
//   $4000-$4013  APU registers
//   $4014        OAM DMA  (intercepted via OamDmaCallback)
//   $4015        APU status
//   $4016-$4017  Controllers / APU frame counter
//   $4020-$FFFF  Cartridge / mapper
class Memory {
public:
    Memory();
    void reset();

    uint8_t read(uint16_t addr);
    void    write(uint16_t addr, uint8_t val);

    void set_ppu_callbacks(MemReadCallback r, MemWriteCallback w);
    void set_apu_callbacks(MemReadCallback r, MemWriteCallback w);
    void set_input_callbacks(MemReadCallback r, MemWriteCallback w);
    void set_cart_callbacks(MemReadCallback r, MemWriteCallback w);
    void set_oam_dma_callback(OamDmaCallback cb) { oam_dma_ = std::move(cb); }

    uint8_t* internal_ram()             { return ram_.data(); }
    const uint8_t* internal_ram() const { return ram_.data(); }

    void serialize(class Serializer& s) const;
    void deserialize(class Deserializer& d);

private:
    std::array<uint8_t, 0x0800> ram_{};
    MemReadCallback  ppu_r_, apu_r_, input_r_, cart_r_;
    MemWriteCallback ppu_w_, apu_w_, input_w_, cart_w_;
    OamDmaCallback   oam_dma_;
};

} // namespace fcemu
