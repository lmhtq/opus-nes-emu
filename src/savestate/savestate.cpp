// savestate.cpp - serialize/deserialize methods for emulator subsystems.
//
// Format is unversioned per-subsystem POD blobs prefixed by a top-level
// header in main.cpp. Each subsystem reads/writes exactly the bytes it
// produced. Layout is little-endian on x86/ARM64; not portable across
// architectures, which is acceptable for a desktop emulator.

#include "fcemu/savestate.h"
#include "fcemu/cpu.h"
#include "fcemu/memory.h"
#include "fcemu/ppu.h"
#include "fcemu/apu.h"
#include "fcemu/cartridge.h"
#include "../cartridge/mappers.h"

namespace fcemu {

// ---------------- CPU ----------------
void Cpu6502::serialize(Serializer& s) const {
    s.write(regs_);
    s.write(nmi_pending_);
    s.write(irq_pending_);
    s.write(cycles_);
}
void Cpu6502::deserialize(Deserializer& d) {
    d.read(regs_);
    d.read(nmi_pending_);
    d.read(irq_pending_);
    d.read(cycles_);
}

// ---------------- Memory ----------------
void Memory::serialize(Serializer& s) const {
    s.write_bytes(ram_.data(), ram_.size());
}
void Memory::deserialize(Deserializer& d) {
    d.read_bytes(ram_.data(), ram_.size());
}

// ---------------- PPU ----------------
//
// Dump every piece of mutable internal state. The framebuffer is transient
// (regenerated each frame) and not saved.
void Ppu::serialize(Serializer& s) const {
    s.write(ppuctrl_); s.write(ppumask_); s.write(ppustatus_); s.write(oam_addr_);
    s.write(v_); s.write(t_); s.write(fine_x_); s.write(w_); s.write(data_buffer_);
    s.write_bytes(vram_.data(),    vram_.size());
    s.write_bytes(palette_.data(), palette_.size());
    s.write_bytes(oam_.data(),     oam_.size());
    s.write(scanline_); s.write(dot_); s.write(odd_frame_);
    s.write(frame_count_); s.write(frame_complete_);
}
void Ppu::deserialize(Deserializer& d) {
    d.read(ppuctrl_); d.read(ppumask_); d.read(ppustatus_); d.read(oam_addr_);
    d.read(v_); d.read(t_); d.read(fine_x_); d.read(w_); d.read(data_buffer_);
    d.read_bytes(vram_.data(),    vram_.size());
    d.read_bytes(palette_.data(), palette_.size());
    d.read_bytes(oam_.data(),     oam_.size());
    d.read(scanline_); d.read(dot_); d.read(odd_frame_);
    d.read(frame_count_); d.read(frame_complete_);
}

// ---------------- APU ----------------
//
// Channel structs are POD-ish (no virtuals, no pointers) so we dump the
// raw bytes. Sample buffer is transient.
void Apu::serialize(Serializer& s) const {
    s.write(pulse1_); s.write(pulse2_); s.write(triangle_); s.write(noise_); s.write(dmc_);
    s.write(sample_rate_); s.write(cycles_per_sample_); s.write(sample_accum_);
    s.write(total_cycles_);
    s.write(frame_step_); s.write(frame_5step_);
    s.write(frame_irq_inhibit_); s.write(frame_irq_); s.write(frame_div_);
}
void Apu::deserialize(Deserializer& d) {
    d.read(pulse1_); d.read(pulse2_); d.read(triangle_); d.read(noise_); d.read(dmc_);
    d.read(sample_rate_); d.read(cycles_per_sample_); d.read(sample_accum_);
    d.read(total_cycles_);
    d.read(frame_step_); d.read(frame_5step_);
    d.read(frame_irq_inhibit_); d.read(frame_irq_); d.read(frame_div_);
    sample_buffer_.clear();
}

// ---------------- Cartridge ----------------
void Cartridge::serialize(Serializer& s) const {
    s.write(mirror_mode_);
    s.write(irq_pending_);
    uint32_t prg_ram_sz = (uint32_t)prg_ram_.size();
    s.write(prg_ram_sz);
    if (prg_ram_sz) s.write_bytes(prg_ram_.data(), prg_ram_sz);
    // CHR-RAM (if present) is mutable game state too.
    uint32_t chr_sz = chr_is_ram_ ? (uint32_t)chr_.size() : 0;
    s.write(chr_sz);
    if (chr_sz) s.write_bytes(chr_.data(), chr_sz);
    if (mapper_) mapper_->save_state(s);
}
void Cartridge::deserialize(Deserializer& d) {
    d.read(mirror_mode_);
    d.read(irq_pending_);
    uint32_t prg_ram_sz; d.read(prg_ram_sz);
    if (prg_ram_sz != prg_ram_.size()) throw std::runtime_error("PRG-RAM size mismatch");
    if (prg_ram_sz) d.read_bytes(prg_ram_.data(), prg_ram_sz);
    uint32_t chr_sz; d.read(chr_sz);
    if (chr_is_ram_) {
        if (chr_sz != chr_.size()) throw std::runtime_error("CHR-RAM size mismatch");
        if (chr_sz) d.read_bytes(chr_.data(), chr_sz);
    } else if (chr_sz != 0) {
        throw std::runtime_error("unexpected CHR data in save");
    }
    if (mapper_) mapper_->load_state(d);
}

// ---------------- Mapper-specific state ----------------
void Mapper1::save_state(Serializer& s) const {
    s.write(shift_); s.write(control_); s.write(chr0_); s.write(chr1_); s.write(prg_);
}
void Mapper1::load_state(Deserializer& d) {
    d.read(shift_); d.read(control_); d.read(chr0_); d.read(chr1_); d.read(prg_);
}
void Mapper2::save_state(Serializer& s) const { s.write(prg_bank_); }
void Mapper2::load_state(Deserializer& d)     { d.read(prg_bank_); }
void Mapper3::save_state(Serializer& s) const { s.write(chr_bank_); }
void Mapper3::load_state(Deserializer& d)     { d.read(chr_bank_); }
void Mapper4::save_state(Serializer& s) const {
    s.write(bank_select_);
    s.write_bytes(bank_regs_, sizeof(bank_regs_));
    s.write(irq_latch_); s.write(irq_counter_); s.write(irq_reload_); s.write(irq_enable_);
    s.write(prg_mode_);  s.write(chr_mode_);
}
void Mapper4::load_state(Deserializer& d) {
    d.read(bank_select_);
    d.read_bytes(bank_regs_, sizeof(bank_regs_));
    d.read(irq_latch_); d.read(irq_counter_); d.read(irq_reload_); d.read(irq_enable_);
    d.read(prg_mode_);  d.read(chr_mode_);
}

} // namespace fcemu
