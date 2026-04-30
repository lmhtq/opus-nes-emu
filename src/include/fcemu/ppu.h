// include/fcemu/ppu.h
#pragma once

#include <cstdint>
#include <functional>
#include <array>

namespace fcemu {

struct PpuRegisters {
    uint8_t ctrl;
    uint8_t mask;
    uint8_t status;
    uint8_t oam_addr;
    uint8_t oam_data;
    uint8_t scroll;
    uint8_t addr;
    uint8_t data;
};

struct FrameBuffer {
    static constexpr int WIDTH = 256;
    static constexpr int HEIGHT = 240;
    uint8_t pixels[WIDTH * HEIGHT * 4];
};

using PpuReadCallback = std::function<uint8_t(uint16_t)>;
using PpuWriteCallback = std::function<void(uint16_t, uint8_t)>;
using NmiCallback = std::function<void()>;

class Ppu {
public:
    Ppu();
    void reset();
    void set_callbacks(PpuReadCallback read, PpuWriteCallback write,
                         NmiCallback nmi);

    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t val);
    void step(int cpu_cycles);
    void step_scanline();
    const FrameBuffer& frame_buffer() const { return frame_buffer_; }
    void signal_nmi();
    bool check_sprite0_hit();
    uint8_t read_status();
    void write_addr(uint8_t val);
    void write_data(uint8_t val);
    uint8_t read_data();
    void write_scroll(uint8_t val);
    void write_oam_data(uint8_t val);

private:
    PpuRegisters regs_;
    FrameBuffer frame_buffer_;
    std::array<uint8_t, 32> palette_;
    uint16_t vram_addr_;
    uint16_t temp_addr_;
    uint8_t fine_x_;
    bool addr_latch_;
    std::array<uint8_t, 256> oam_;
    std::array<uint8_t, 32> secondary_oam_;
    uint16_t bg_shift_lo_;
    uint16_t bg_shift_hi_;
    uint8_t bg_attr_lo_;
    uint8_t bg_attr_hi_;
    int scanline_;
    int dot_;
    bool even_frame_;
    PpuReadCallback read_;
    PpuWriteCallback write_;
    NmiCallback nmi_callback_;

    void render_scanline(int y);
    void evaluate_sprites(int y);
    void fetch_bg_tile(int x);
    uint8_t read_palette(uint16_t addr);
    void write_palette(uint16_t addr, uint8_t val);
};

} // namespace fcemu
