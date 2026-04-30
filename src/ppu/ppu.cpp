// ppu.cpp - PPU emulator (partial implementation)
#include "fcemu/ppu.h"
#include <cstring>
#include <cstdio>

namespace fcemu {

Ppu::Ppu()
    : regs_{}, frame_buffer_{}, vram_addr_(0), temp_addr_(0),
      fine_x_(0), addr_latch_(false), scanline_(0), dot_(0),
      even_frame_(true) {
    std::memset(&frame_buffer_, 0, sizeof(frame_buffer_));
    std::memset(oam_, 0, sizeof(oam_));
    std::memset(secondary_oam_, 0, sizeof(secondary_oam_));
    std::memset(palette_, 0, sizeof(palette_));
}

void Ppu::reset() {
    regs_.ctrl = 0;
    regs_.mask = 0;
    regs_.status = 0;
    regs_.oam_addr = 0;
    vram_addr_ = 0;
    temp_addr_ = 0;
    fine_x_ = 0;
    addr_latch_ = false;
    scanline_ = 0;
    dot_ = 0;
    even_frame_ = true;
}

void Ppu::set_callbacks(PpuReadCallback read, PpuWriteCallback write,
                        NmiCallback nmi) {
    read_ = read;
    write_ = write;
    nmi_callback_ = nmi;
}

uint8_t Ppu::cpu_read(uint16_t addr) {
    addr &= 0x0007;  // Mirror $2000-$2007
    switch (addr) {
        case 0: return regs_.ctrl;
        case 1: return regs_.mask;
        case 2: return read_status();
        case 3: return regs_.oam_addr;
        case 4: return regs_.oam_data;
        case 5: return regs_.scroll;
        case 6: return regs_.addr;
        case 7: return read_data();
    }
    return 0;
}

void Ppu::cpu_write(uint16_t addr, uint8_t val) {
    addr &= 0x0007;
    switch (addr) {
        case 0: regs_.ctrl = val; break;
        case 1: regs_.mask = val; break;
        case 2: break; // Status is read-only
        case 3: regs_.oam_addr = val; break;
        case 4: write_oam_data(val); break;
        case 5: write_scroll(val); break;
        case 6: write_addr(val); break;
        case 7: write_data(val); break;
    }
}

void Ppu::step(int cpu_cycles) {
    // PPU runs at 3x CPU speed
    for (int i = 0; i < cpu_cycles * 3; ++i) {
        step_scanline();
    }
}

void Ppu::step_scanline() {
    if (scanline_ >= 0 && scanline_ <= 239) {
        if (dot_ >= 1 && dot_ <= 256) {
            // Active rendering
            render_scanline(scanline_);
        }
    } else if (scanline_ == 241) {
        if (dot_ == 1) {
            regs_.status |= 0x80;  // Set VBlank flag
            if (regs_.ctrl & 0x80) {
                if (nmi_callback_) nmi_callback_();
            }
        }
    } else if (scanline_ == 261) {
        if (dot_ == 1) {
            regs_.status &= ~0x80;  // Clear VBlank
        }
    }

    dot_++;
    if (dot_ > 340) {
        dot_ = 0;
        scanline_++;
        if (scanline_ > 261) {
            scanline_ = 0;
            even_frame_ = !even_frame_;
        }
    }
}

void Ppu::render_scanline(int y) {
    // Partial: just fill with a color
    for (int x = 0; x < 256; ++x) {
        int idx = (y * 256 + x) * 4;
        frame_buffer_.pixels[idx + 0] = 0x00;  // R
        frame_buffer_.pixels[idx + 1] = 0x00;  // G
        frame_buffer_.pixels[idx + 2] = 0x80;  // B
        frame_buffer_.pixels[idx + 3] = 0xFF;  // A
    }
}

const Ppu::FrameBuffer& Ppu::frame_buffer() const { return frame_buffer_; }

uint8_t Ppu::read_status() {
    uint8_t status = regs_.status;
    regs_.status &= ~0x80;  // Clear VBlank on read
    addr_latch_ = false;  // Clear address latch
    return status;
}

void Ppu::write_scroll(uint8_t val) {
    if (!addr_latch_) {
        regs_.scroll = val;
        fine_x_ = val & 0x07;
        addr_latch_ = true;
    } else {
        regs_.scroll = val;
        addr_latch_ = false;
    }
}

void Ppu::write_addr(uint8_t val) {
    if (!addr_latch_) {
        temp_addr_ = (temp_addr_ & 0x00FF) | (val << 8);
        addr_latch_ = true;
    } else {
        temp_addr_ = (temp_addr_ & 0xFF00) | val;
        vram_addr_ = temp_addr_;
        addr_latch_ = false;
    }
}

void Ppu::write_data(uint8_t val) {
    // TODO: Write to VRAM at vram_addr_
    vram_addr_ += (regs_.ctrl & 0x04) ? 32 : 1;
}

uint8_t Ppu::read_data() {
    // TODO: Implement buffered read
    uint8_t val = 0;  // read_(vram_addr_) for real
    vram_addr_ += (regs_.ctrl & 0x04) ? 32 : 1;
    return val;
}

void Ppu::write_oam_data(uint8_t val) {
    oam_[regs_.oam_addr++] = val;
}

void Ppu::signal_nmi() {
    if (nmi_callback_) nmi_callback_();
}

bool Ppu::check_sprite0_hit() {
    return (regs_.status & 0x40) != 0;
}

} // namespace fcemu
