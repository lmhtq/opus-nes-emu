// include/fcemu/ppu.h
#pragma once

#include <cstdint>
#include <functional>
#include <array>

namespace fcemu {

class Cartridge;

struct FrameBuffer {
    static constexpr int WIDTH  = 256;
    static constexpr int HEIGHT = 240;
    uint8_t pixels[WIDTH * HEIGHT * 4]; // RGBA8
};

using NmiCallback = std::function<void()>;

class Ppu {
public:
    Ppu();
    void reset();

    void set_cartridge(Cartridge* cart) { cart_ = cart; }
    void set_nmi_callback(NmiCallback cb) { nmi_ = std::move(cb); }

    uint8_t cpu_read(uint16_t addr);
    void    cpu_write(uint16_t addr, uint8_t val);

    // Step 3 PPU cycles per CPU cycle.
    void step(int cpu_cycles);

    // Inject a full OAM page during DMA.
    void oam_dma_write(const uint8_t* page256);

    const FrameBuffer& frame() const { return frame_; }
    bool frame_complete() { bool v = frame_complete_; frame_complete_ = false; return v; }
    int  frame_count() const { return frame_count_; }

    // Inspection (read-only) used by analyzer / debugger.
    const uint8_t* oam()      const { return oam_.data(); }
    const uint8_t* palette()  const { return palette_.data(); }
    int  scanline() const { return scanline_; }
    int  dot() const      { return dot_; }

private:
    Cartridge*  cart_ = nullptr;
    NmiCallback nmi_;

    // Registers.
    uint8_t ppuctrl_ = 0;
    uint8_t ppumask_ = 0;
    uint8_t ppustatus_ = 0;
    uint8_t oam_addr_  = 0;

    // Loopy registers.
    uint16_t v_ = 0, t_ = 0;
    uint8_t  fine_x_ = 0;
    bool     w_ = false;        // first/second write toggle
    uint8_t  data_buffer_ = 0;

    // Internal memory.
    std::array<uint8_t, 0x800>  vram_{};      // 2KB nametable RAM
    std::array<uint8_t, 32>     palette_{};
    std::array<uint8_t, 256>    oam_{};

    // Timing.
    int scanline_ = 261;
    int dot_      = 0;
    bool odd_frame_ = false;
    int frame_count_ = 0;
    bool frame_complete_ = false;

    FrameBuffer frame_{};

    // Helpers.
    bool rendering_enabled() const { return (ppumask_ & 0x18) != 0; }
    uint8_t  read_vram(uint16_t addr);
    void     write_vram(uint16_t addr, uint8_t val);
    uint16_t mirror_nametable(uint16_t addr) const;
    void     render_scanline(int y);
    void     increment_v_y();
    static const uint32_t NES_PALETTE[64];
};

} // namespace fcemu
