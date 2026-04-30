// ppu.cpp - Picture Processing Unit. Scanline-accurate background + sprite renderer.
#include "fcemu/ppu.h"
#include "fcemu/cartridge.h"

#include <algorithm>
#include <cstring>

namespace fcemu {

const uint32_t Ppu::NES_PALETTE[64] = {
    0x666666FF,0x002A88FF,0x1412A7FF,0x3B00A4FF,0x5C007EFF,0x6E0040FF,0x6C0600FF,0x561D00FF,
    0x333500FF,0x0B4800FF,0x005200FF,0x004F08FF,0x00404DFF,0x000000FF,0x000000FF,0x000000FF,
    0xADADADFF,0x155FD9FF,0x4240FFFF,0x7527FEFF,0xA01ACCFF,0xB71E7BFF,0xB53120FF,0x994E00FF,
    0x6B6D00FF,0x388700FF,0x0C9300FF,0x008F32FF,0x007C8DFF,0x000000FF,0x000000FF,0x000000FF,
    0xFFFEFFFF,0x64B0FFFF,0x9290FFFF,0xC676FFFF,0xF36AFFFF,0xFE6ECCFF,0xFE8170FF,0xEA9E22FF,
    0xBCBE00FF,0x88D800FF,0x5CE430FF,0x45E082FF,0x48CDDEFF,0x4F4F4FFF,0x000000FF,0x000000FF,
    0xFFFEFFFF,0xC0DFFFFF,0xD3D2FFFF,0xE8C8FFFF,0xFBC2FFFF,0xFEC4EAFF,0xFECCC5FF,0xF7D8A5FF,
    0xE4E594FF,0xCFEF96FF,0xBDF4ABFF,0xB3F3CCFF,0xB5EBF2FF,0xB8B8B8FF,0x000000FF,0x000000FF
};

Ppu::Ppu() { reset(); }

void Ppu::reset() {
    ppuctrl_ = ppumask_ = ppustatus_ = oam_addr_ = 0;
    v_ = t_ = 0; fine_x_ = 0; w_ = false; data_buffer_ = 0;
    vram_.fill(0); palette_.fill(0); oam_.fill(0);
    scanline_ = 261; dot_ = 0; odd_frame_ = false;
    frame_count_ = 0; frame_complete_ = false;
    std::memset(frame_.pixels, 0, sizeof(frame_.pixels));
}

uint16_t Ppu::mirror_nametable(uint16_t addr) const {
    addr &= 0x0FFF;          // 4KB region
    int table = addr >> 10;  // 0..3
    int offset = addr & 0x03FF;
    MirrorMode m = cart_ ? cart_->mirror_mode() : MirrorMode::Horizontal;
    int mapped = 0;
    switch (m) {
        case MirrorMode::Horizontal:  mapped = (table / 2) * 0x400; break;
        case MirrorMode::Vertical:    mapped = (table % 2) * 0x400; break;
        case MirrorMode::Single0:     mapped = 0x000; break;
        case MirrorMode::Single1:     mapped = 0x400; break;
        case MirrorMode::FourScreen:  mapped = (table & 1) * 0x400; break;
    }
    return (uint16_t)(mapped + offset);
}

uint8_t Ppu::read_vram(uint16_t addr) {
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        return cart_ ? cart_->ppu_read(addr) : 0;
    }
    if (addr < 0x3F00) {
        return vram_[mirror_nametable(addr)];
    }
    addr &= 0x1F;
    if ((addr & 0x13) == 0x10) addr &= ~0x10; // mirrors of $3F00/04/08/0C
    return palette_[addr];
}

void Ppu::write_vram(uint16_t addr, uint8_t val) {
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        if (cart_) cart_->ppu_write(addr, val);
        return;
    }
    if (addr < 0x3F00) {
        vram_[mirror_nametable(addr)] = val;
        return;
    }
    addr &= 0x1F;
    if ((addr & 0x13) == 0x10) addr &= ~0x10;
    palette_[addr] = val & 0x3F;
}

uint8_t Ppu::cpu_read(uint16_t addr) {
    addr &= 0x07;
    switch (addr) {
        case 2: {
            uint8_t r = ppustatus_;
            ppustatus_ &= ~0x80; // clear vblank
            w_ = false;
            return r;
        }
        case 4: return oam_[oam_addr_];
        case 7: {
            uint8_t r;
            uint16_t a = v_ & 0x3FFF;
            if (a < 0x3F00) {
                r = data_buffer_;
                data_buffer_ = read_vram(a);
            } else {
                r = read_vram(a);
                data_buffer_ = read_vram(a - 0x1000);
            }
            v_ += (ppuctrl_ & 0x04) ? 32 : 1;
            v_ &= 0x3FFF;
            return r;
        }
    }
    return 0;
}

void Ppu::cpu_write(uint16_t addr, uint8_t val) {
    addr &= 0x07;
    switch (addr) {
        case 0:
            ppuctrl_ = val;
            t_ = (t_ & 0xF3FF) | ((val & 0x03) << 10);
            break;
        case 1: ppumask_ = val; break;
        case 3: oam_addr_ = val; break;
        case 4: oam_[oam_addr_++] = val; break;
        case 5:
            if (!w_) {
                fine_x_ = val & 0x07;
                t_ = (t_ & 0xFFE0) | (val >> 3);
            } else {
                t_ = (t_ & 0x8FFF) | ((val & 0x07) << 12);
                t_ = (t_ & 0xFC1F) | ((val & 0xF8) << 2);
            }
            w_ = !w_;
            break;
        case 6:
            if (!w_) {
                t_ = (t_ & 0x00FF) | ((val & 0x3F) << 8);
            } else {
                t_ = (t_ & 0xFF00) | val;
                v_ = t_;
            }
            w_ = !w_;
            break;
        case 7:
            write_vram(v_ & 0x3FFF, val);
            v_ += (ppuctrl_ & 0x04) ? 32 : 1;
            v_ &= 0x3FFF;
            break;
    }
}

void Ppu::oam_dma_write(const uint8_t* page) {
    for (int i = 0; i < 256; ++i) {
        oam_[(uint8_t)(oam_addr_ + i)] = page[i];
    }
}

void Ppu::increment_v_y() {
    if ((v_ & 0x7000) != 0x7000) {
        v_ += 0x1000;
    } else {
        v_ &= ~0x7000;
        int y = (v_ & 0x03E0) >> 5;
        if (y == 29) {
            y = 0;
            v_ ^= 0x0800;
        } else if (y == 31) {
            y = 0;
        } else {
            ++y;
        }
        v_ = (v_ & ~0x03E0) | (y << 5);
    }
}

void Ppu::render_scanline(int y) {
    if (y < 0 || y >= 240) return;
    bool show_bg     = (ppumask_ & 0x08) != 0;
    bool show_sp     = (ppumask_ & 0x10) != 0;
    bool clip_bg_l8  = (ppumask_ & 0x02) == 0;
    bool clip_sp_l8  = (ppumask_ & 0x04) == 0;
    bool greyscale   = (ppumask_ & 0x01) != 0;

    // --- Background pixel buffer (palette index 0..3 + attribute) ---
    uint8_t bg_idx[256] = {0};       // colour index 0..3 (0 = transparent)
    uint8_t bg_pal[256] = {0};       // palette group 0..3

    if (show_bg) {
        // Use current v register; copy horizontal bits from t at start of each visible scanline.
        if (rendering_enabled()) {
            v_ = (v_ & ~0x041F) | (t_ & 0x041F);
        }
        uint16_t bg_pt = (ppuctrl_ & 0x10) ? 0x1000 : 0x0000;
        int fineY = (v_ >> 12) & 7;
        int x = 0;
        while (x < 256) {
            int coarseX = v_ & 0x1F;
            int coarseY = (v_ >> 5) & 0x1F;
            int ntSel   = (v_ >> 10) & 0x03;
            uint16_t nt = 0x2000 | (ntSel << 10) | (coarseY << 5) | coarseX;
            uint8_t  tile = read_vram(nt);
            uint16_t at = 0x23C0 | (ntSel << 10) | ((coarseY >> 2) << 3) | (coarseX >> 2);
            uint8_t  attr = read_vram(at);
            int shift = ((coarseY & 2) << 1) | (coarseX & 2);
            uint8_t pal = (attr >> shift) & 0x03;
            uint16_t base = bg_pt + tile * 16 + fineY;
            uint8_t lo = read_vram(base);
            uint8_t hi = read_vram(base + 8);

            int start = (x == 0) ? fine_x_ : 0;
            for (int p = start; p < 8 && x < 256; ++p, ++x) {
                int bit = 7 - p;
                int c = ((lo >> bit) & 1) | (((hi >> bit) & 1) << 1);
                bg_idx[x] = (uint8_t)c;
                bg_pal[x] = pal;
            }

            // Coarse-X increment with horizontal nametable swap.
            if ((v_ & 0x1F) == 31) {
                v_ &= ~0x1F;
                v_ ^= 0x0400;
            } else {
                v_ += 1;
            }
        }
        if (rendering_enabled()) increment_v_y();
    }

    // --- Sprite evaluation: collect up to 8 sprites for this scanline ---
    struct SpriteSlot {
        int x;
        uint8_t pattern_lo, pattern_hi, attr;
        bool sprite0;
    };
    SpriteSlot slots[8];
    int n_slots = 0;
    bool overflow = false;
    int sp_height = (ppuctrl_ & 0x20) ? 16 : 8;
    if (show_sp) {
        for (int i = 0; i < 64; ++i) {
            int sy = oam_[i*4 + 0] + 1;
            if (y < sy || y >= sy + sp_height) continue;
            if (n_slots == 8) { overflow = true; break; }
            int row = y - sy;
            uint8_t tile = oam_[i*4 + 1];
            uint8_t attr = oam_[i*4 + 2];
            int sx       = oam_[i*4 + 3];
            bool flipH   = attr & 0x40;
            bool flipV   = attr & 0x80;
            int patternRow = flipV ? (sp_height - 1 - row) : row;
            uint16_t base;
            if (sp_height == 16) {
                uint16_t pt = (tile & 1) ? 0x1000 : 0x0000;
                int t = tile & 0xFE;
                if (patternRow >= 8) { t += 1; patternRow -= 8; }
                base = pt + t * 16 + patternRow;
            } else {
                uint16_t pt = (ppuctrl_ & 0x08) ? 0x1000 : 0x0000;
                base = pt + tile * 16 + patternRow;
            }
            uint8_t lo = read_vram(base);
            uint8_t hi = read_vram(base + 8);
            if (flipH) {
                auto rev = [](uint8_t v){
                    v = (v & 0xF0) >> 4 | (v & 0x0F) << 4;
                    v = (v & 0xCC) >> 2 | (v & 0x33) << 2;
                    v = (v & 0xAA) >> 1 | (v & 0x55) << 1;
                    return v;
                };
                lo = rev(lo); hi = rev(hi);
            }
            slots[n_slots++] = { sx, lo, hi, attr, i == 0 };
        }
    }
    if (overflow) ppustatus_ |= 0x20;

    // --- Compose final pixel row ---
    uint8_t universal_bg = palette_[0] & 0x3F;
    for (int x = 0; x < 256; ++x) {
        int bg_color = bg_idx[x];
        if (clip_bg_l8 && x < 8) bg_color = 0;
        uint8_t bg_palidx = (bg_color == 0) ? universal_bg
                          : palette_[(bg_pal[x] << 2) | bg_color] & 0x3F;

        int sp_color = 0;
        uint8_t sp_palidx = 0;
        bool sp_priority_front = false;
        bool is_sp0 = false;

        for (int i = 0; i < n_slots; ++i) {
            int dx = x - slots[i].x;
            if (dx < 0 || dx >= 8) continue;
            int bit = 7 - dx;
            int c = ((slots[i].pattern_lo >> bit) & 1) |
                    (((slots[i].pattern_hi >> bit) & 1) << 1);
            if (c == 0) continue;
            if (clip_sp_l8 && x < 8) continue;
            sp_color = c;
            uint8_t pal = slots[i].attr & 0x03;
            sp_palidx = palette_[0x10 + (pal << 2) + c] & 0x3F;
            sp_priority_front = (slots[i].attr & 0x20) == 0;
            is_sp0 = slots[i].sprite0;
            break;
        }

        // sprite-0 hit
        if (is_sp0 && bg_color != 0 && sp_color != 0 && x != 255 && show_bg && show_sp) {
            ppustatus_ |= 0x40;
        }

        uint8_t final_idx;
        if (bg_color == 0 && sp_color == 0)        final_idx = universal_bg;
        else if (bg_color == 0)                    final_idx = sp_palidx;
        else if (sp_color == 0)                    final_idx = bg_palidx;
        else                                       final_idx = sp_priority_front ? sp_palidx : bg_palidx;

        if (greyscale) final_idx &= 0x30;
        uint32_t rgba = NES_PALETTE[final_idx & 0x3F];
        int p = (y * 256 + x) * 4;
        frame_.pixels[p+0] = (rgba >> 24) & 0xFF;
        frame_.pixels[p+1] = (rgba >> 16) & 0xFF;
        frame_.pixels[p+2] = (rgba >>  8) & 0xFF;
        frame_.pixels[p+3] = 0xFF;
    }
}

void Ppu::step(int cpu_cycles) {
    int dots = cpu_cycles * 3;
    for (int i = 0; i < dots; ++i) {
        // Visible scanlines 0..239: render at dot 257 (after fetching tiles).
        if (scanline_ >= 0 && scanline_ < 240 && dot_ == 257) {
            render_scanline(scanline_);
        }

        // Pre-render scanline (-1 == 261): copy v from t at dots 280..304.
        if (scanline_ == 261 && dot_ == 304 && rendering_enabled()) {
            v_ = (v_ & ~0x7BE0) | (t_ & 0x7BE0);
        }

        // Mapper IRQ tick on visible scanlines (approximation of A12 rising edge).
        if (cart_ && (scanline_ >= 0 && scanline_ < 240) && dot_ == 260 && rendering_enabled()) {
            cart_->notify_scanline();
        }

        if (scanline_ == 241 && dot_ == 1) {
            ppustatus_ |= 0x80; // VBlank
            frame_complete_ = true;
            ++frame_count_;
            if ((ppuctrl_ & 0x80) && nmi_) nmi_();
        }
        if (scanline_ == 261 && dot_ == 1) {
            ppustatus_ &= ~(0x80 | 0x40 | 0x20); // clear vblank/sprite0/overflow
        }

        ++dot_;
        if (dot_ > 340) {
            dot_ = 0;
            ++scanline_;
            if (scanline_ > 261) {
                scanline_ = 0;
                odd_frame_ = !odd_frame_;
            }
        }
    }
}

} // namespace fcemu
