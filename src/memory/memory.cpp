// memory.cpp - CPU bus router.
#include "fcemu/memory.h"
#include <cstring>

namespace fcemu {

Memory::Memory() { ram_.fill(0); }

void Memory::reset() { ram_.fill(0); }

uint8_t Memory::read(uint16_t addr) {
    if (addr < 0x2000) {
        return ram_[addr & 0x07FF];
    }
    if (addr < 0x4000) {
        return ppu_r_ ? ppu_r_(0x2000 | (addr & 0x0007)) : 0;
    }
    if (addr < 0x4020) {
        if (addr == 0x4016 || addr == 0x4017) {
            return input_r_ ? input_r_(addr) : 0;
        }
        return apu_r_ ? apu_r_(addr) : 0;
    }
    return cart_r_ ? cart_r_(addr) : 0;
}

void Memory::write(uint16_t addr, uint8_t val) {
    if (addr < 0x2000) {
        ram_[addr & 0x07FF] = val;
        return;
    }
    if (addr < 0x4000) {
        if (ppu_w_) ppu_w_(0x2000 | (addr & 0x0007), val);
        return;
    }
    if (addr < 0x4020) {
        if (addr == 0x4014) {
            if (oam_dma_) oam_dma_(val);
            return;
        }
        if (addr == 0x4016) {
            if (input_w_) input_w_(addr, val);
            return;
        }
        // $4017 is shared between APU frame counter and second controller strobe.
        // Forward to APU; controller strobe is taken from $4016 only.
        if (apu_w_) apu_w_(addr, val);
        return;
    }
    if (cart_w_) cart_w_(addr, val);
}

void Memory::set_ppu_callbacks(MemReadCallback r, MemWriteCallback w)   { ppu_r_=std::move(r); ppu_w_=std::move(w); }
void Memory::set_apu_callbacks(MemReadCallback r, MemWriteCallback w)   { apu_r_=std::move(r); apu_w_=std::move(w); }
void Memory::set_input_callbacks(MemReadCallback r, MemWriteCallback w) { input_r_=std::move(r); input_w_=std::move(w); }
void Memory::set_cart_callbacks(MemReadCallback r, MemWriteCallback w)  { cart_r_=std::move(r); cart_w_=std::move(w); }

} // namespace fcemu
