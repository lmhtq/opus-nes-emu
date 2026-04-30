// mappers.cpp - Concrete iNES mapper implementations.
#include "mappers.h"

namespace fcemu {

namespace {
inline size_t safe_mod(size_t v, size_t m) { return m ? (v % m) : 0; }
}

// ---------------- Mapper 0 (NROM) ----------------

uint8_t Mapper0::cpu_read(uint16_t addr) {
    auto& prg = cart_.prg_rom();
    if (addr >= 0x6000 && addr < 0x8000) {
        auto& ram = cart_.prg_ram();
        return ram.empty() ? 0 : ram[(addr - 0x6000) % ram.size()];
    }
    if (addr >= 0x8000) {
        size_t off = addr - 0x8000;
        // 16 KB PRG mirrors at $C000.
        if (prg.size() == 16 * 1024) off &= 0x3FFF;
        return prg.empty() ? 0 : prg[off % prg.size()];
    }
    return 0;
}
void Mapper0::cpu_write(uint16_t addr, uint8_t val) {
    if (addr >= 0x6000 && addr < 0x8000) {
        auto& ram = cart_.prg_ram();
        if (!ram.empty()) ram[(addr - 0x6000) % ram.size()] = val;
    }
    // PRG ROM ignores writes.
}
uint8_t Mapper0::ppu_read(uint16_t addr) {
    auto& chr = cart_.chr();
    return chr.empty() ? 0 : chr[addr & 0x1FFF];
}
void Mapper0::ppu_write(uint16_t addr, uint8_t val) {
    if (cart_.chr_is_ram()) {
        auto& chr = cart_.chr();
        if (!chr.empty()) chr[addr & 0x1FFF] = val;
    }
}

// ---------------- Mapper 1 (MMC1) ----------------

void Mapper1::reset() {
    shift_ = 0x10;
    control_ = 0x0C;     // PRG: fix last bank at $C000
    chr0_ = chr1_ = prg_ = 0;
    apply_mirror();
}

void Mapper1::apply_mirror() {
    switch (control_ & 0x03) {
        case 0: cart_.set_mirror_mode(MirrorMode::Single0);   break;
        case 1: cart_.set_mirror_mode(MirrorMode::Single1);   break;
        case 2: cart_.set_mirror_mode(MirrorMode::Vertical);  break;
        case 3: cart_.set_mirror_mode(MirrorMode::Horizontal);break;
    }
}

void Mapper1::write_register(uint16_t addr, uint8_t val) {
    int reg = (addr >> 13) & 0x03; // $8000,$A000,$C000,$E000
    switch (reg) {
        case 0: control_ = val & 0x1F; apply_mirror(); break;
        case 1: chr0_    = val & 0x1F; break;
        case 2: chr1_    = val & 0x1F; break;
        case 3: prg_     = val & 0x0F; break;
    }
}

size_t Mapper1::prg_offset(int slot) const {
    auto& prg = cart_.prg_rom();
    int banks = (int)(prg.size() / (16 * 1024));
    if (banks == 0) return 0;
    int mode = (control_ >> 2) & 0x03;
    int bank = prg_ & 0x0F;
    int sel0, sel1;
    if (mode == 2) {                  // fix first at $8000
        sel0 = 0; sel1 = bank;
    } else if (mode == 3) {           // fix last at $C000
        sel0 = bank; sel1 = banks - 1;
    } else {                          // 32KB switch
        sel0 = bank & ~1; sel1 = sel0 + 1;
    }
    return ((slot == 0) ? sel0 : sel1) * 16 * 1024;
}

size_t Mapper1::chr_offset(int slot) const {
    auto& chr = cart_.chr();
    int banks_4k = (int)(chr.size() / (4 * 1024));
    if (banks_4k == 0) return 0;
    if (control_ & 0x10) {                          // 4KB mode
        int b = (slot == 0 ? chr0_ : chr1_) & 0x1F;
        return ((size_t)b % banks_4k) * 4 * 1024;
    } else {                                        // 8KB mode
        int b = (chr0_ & 0x1E);
        return ((size_t)(b + slot) % banks_4k) * 4 * 1024;
    }
}

uint8_t Mapper1::cpu_read(uint16_t addr) {
    if (addr >= 0x6000 && addr < 0x8000) {
        auto& ram = cart_.prg_ram();
        return ram.empty() ? 0 : ram[(addr - 0x6000) % ram.size()];
    }
    if (addr >= 0x8000) {
        auto& prg = cart_.prg_rom();
        if (prg.empty()) return 0;
        size_t base = (addr < 0xC000) ? prg_offset(0) : prg_offset(1);
        size_t off  = (addr - 0x8000) & 0x3FFF;
        return prg[(base + off) % prg.size()];
    }
    return 0;
}

void Mapper1::cpu_write(uint16_t addr, uint8_t val) {
    if (addr >= 0x6000 && addr < 0x8000) {
        auto& ram = cart_.prg_ram();
        if (!ram.empty()) ram[(addr - 0x6000) % ram.size()] = val;
        return;
    }
    if (addr < 0x8000) return;
    if (val & 0x80) {                       // reset shift register
        shift_ = 0x10;
        control_ |= 0x0C;
        apply_mirror();
        return;
    }
    bool last = (shift_ & 0x01) != 0;
    shift_ = (shift_ >> 1) | ((val & 1) << 4);
    if (last) {
        write_register(addr, shift_);
        shift_ = 0x10;
    }
}

uint8_t Mapper1::ppu_read(uint16_t addr) {
    auto& chr = cart_.chr();
    if (chr.empty()) return 0;
    int slot = (addr & 0x1000) ? 1 : 0;
    size_t base = chr_offset(slot);
    size_t off  = addr & 0x0FFF;
    return chr[(base + off) % chr.size()];
}

void Mapper1::ppu_write(uint16_t addr, uint8_t val) {
    if (!cart_.chr_is_ram()) return;
    auto& chr = cart_.chr();
    int slot = (addr & 0x1000) ? 1 : 0;
    size_t base = chr_offset(slot);
    size_t off  = addr & 0x0FFF;
    if (!chr.empty()) chr[(base + off) % chr.size()] = val;
}

// ---------------- Mapper 2 (UxROM) ----------------

uint8_t Mapper2::cpu_read(uint16_t addr) {
    auto& prg = cart_.prg_rom();
    if (addr >= 0x6000 && addr < 0x8000) {
        auto& ram = cart_.prg_ram();
        return ram.empty() ? 0 : ram[(addr - 0x6000) % ram.size()];
    }
    if (addr >= 0x8000) {
        if (prg.empty()) return 0;
        int banks = (int)(prg.size() / (16 * 1024));
        int sel = (addr < 0xC000) ? (prg_bank_ % banks) : (banks - 1);
        return prg[sel * 16 * 1024 + ((addr - 0x8000) & 0x3FFF)];
    }
    return 0;
}
void Mapper2::cpu_write(uint16_t addr, uint8_t val) {
    if (addr >= 0x8000) prg_bank_ = val & 0x0F;
    else if (addr >= 0x6000) {
        auto& ram = cart_.prg_ram();
        if (!ram.empty()) ram[(addr - 0x6000) % ram.size()] = val;
    }
}
uint8_t Mapper2::ppu_read(uint16_t addr) {
    auto& chr = cart_.chr();
    return chr.empty() ? 0 : chr[addr & 0x1FFF];
}
void Mapper2::ppu_write(uint16_t addr, uint8_t val) {
    auto& chr = cart_.chr();
    if (!chr.empty()) chr[addr & 0x1FFF] = val; // CHR is RAM in UxROM
}

// ---------------- Mapper 3 (CNROM) ----------------

uint8_t Mapper3::cpu_read(uint16_t addr) {
    auto& prg = cart_.prg_rom();
    if (addr >= 0x8000) {
        size_t off = addr - 0x8000;
        if (prg.size() == 16 * 1024) off &= 0x3FFF;
        return prg.empty() ? 0 : prg[off % prg.size()];
    }
    return 0;
}
void Mapper3::cpu_write(uint16_t addr, uint8_t val) {
    if (addr >= 0x8000) chr_bank_ = val & 0x03;
}
uint8_t Mapper3::ppu_read(uint16_t addr) {
    auto& chr = cart_.chr();
    if (chr.empty()) return 0;
    int banks = (int)(chr.size() / (8 * 1024));
    int sel = banks ? (chr_bank_ % banks) : 0;
    return chr[sel * 8 * 1024 + (addr & 0x1FFF)];
}
void Mapper3::ppu_write(uint16_t addr, uint8_t val) {
    if (!cart_.chr_is_ram()) return;
    auto& chr = cart_.chr();
    if (!chr.empty()) chr[addr & 0x1FFF] = val;
}

// ---------------- Mapper 4 (MMC3) ----------------

void Mapper4::reset() {
    bank_select_ = 0;
    for (auto& r : bank_regs_) r = 0;
    irq_latch_ = irq_counter_ = 0;
    irq_reload_ = false;
    irq_enable_ = false;
    prg_mode_ = chr_mode_ = false;
}

size_t Mapper4::prg_window(int slot) const {
    auto& prg = cart_.prg_rom();
    int banks = (int)(prg.size() / (8 * 1024));
    if (banks == 0) return 0;
    int last  = banks - 1;
    int sel;
    int r6 = bank_regs_[6] % banks;
    int r7 = bank_regs_[7] % banks;
    if (!prg_mode_) {
        // $8000=R6, $A000=R7, $C000=last-1, $E000=last
        sel = (slot == 0) ? r6 : (slot == 1) ? r7 : (slot == 2) ? (last - 1) : last;
    } else {
        // $8000=last-1, $A000=R7, $C000=R6, $E000=last
        sel = (slot == 0) ? (last - 1) : (slot == 1) ? r7 : (slot == 2) ? r6 : last;
    }
    if (sel < 0) sel = 0;
    return (size_t)sel * 8 * 1024;
}

size_t Mapper4::chr_window(int slot) const {
    auto& chr = cart_.chr();
    int banks = (int)(chr.size() / 1024);
    if (banks == 0) return 0;
    // 8 1KB slots, mapped from 6 mapper regs (R0/R1 = 2KB, R2..R5 = 1KB).
    int sel;
    auto wrap = [&](int v){ return (v % banks + banks) % banks; };
    if (!chr_mode_) {
        // $0000-$07FF = R0&~1 / R0|1 (2KB)
        // $0800-$0FFF = R1&~1 / R1|1 (2KB)
        // $1000-$1FFF = R2..R5 (4×1KB)
        switch (slot) {
            case 0: sel = wrap(bank_regs_[0] & 0xFE);     break;
            case 1: sel = wrap((bank_regs_[0] & 0xFE)|1); break;
            case 2: sel = wrap(bank_regs_[1] & 0xFE);     break;
            case 3: sel = wrap((bank_regs_[1] & 0xFE)|1); break;
            case 4: sel = wrap(bank_regs_[2]);            break;
            case 5: sel = wrap(bank_regs_[3]);            break;
            case 6: sel = wrap(bank_regs_[4]);            break;
            default:sel = wrap(bank_regs_[5]);            break;
        }
    } else {
        switch (slot) {
            case 0: sel = wrap(bank_regs_[2]);            break;
            case 1: sel = wrap(bank_regs_[3]);            break;
            case 2: sel = wrap(bank_regs_[4]);            break;
            case 3: sel = wrap(bank_regs_[5]);            break;
            case 4: sel = wrap(bank_regs_[0] & 0xFE);     break;
            case 5: sel = wrap((bank_regs_[0] & 0xFE)|1); break;
            case 6: sel = wrap(bank_regs_[1] & 0xFE);     break;
            default:sel = wrap((bank_regs_[1] & 0xFE)|1); break;
        }
    }
    return (size_t)sel * 1024;
}

uint8_t Mapper4::cpu_read(uint16_t addr) {
    if (addr >= 0x6000 && addr < 0x8000) {
        auto& ram = cart_.prg_ram();
        return ram.empty() ? 0 : ram[(addr - 0x6000) % ram.size()];
    }
    if (addr >= 0x8000) {
        auto& prg = cart_.prg_rom();
        if (prg.empty()) return 0;
        int slot = (addr - 0x8000) >> 13; // 0..3
        size_t base = prg_window(slot);
        size_t off  = (addr - 0x8000) & 0x1FFF;
        return prg[(base + off) % prg.size()];
    }
    return 0;
}

void Mapper4::cpu_write(uint16_t addr, uint8_t val) {
    if (addr >= 0x6000 && addr < 0x8000) {
        auto& ram = cart_.prg_ram();
        if (!ram.empty()) ram[(addr - 0x6000) % ram.size()] = val;
        return;
    }
    if (addr < 0x8000) return;

    bool even = (addr & 1) == 0;
    if (addr <= 0x9FFF) {
        if (even) {
            bank_select_ = val;
            prg_mode_ = (val & 0x40) != 0;
            chr_mode_ = (val & 0x80) != 0;
        } else {
            int reg = bank_select_ & 0x07;
            bank_regs_[reg] = val;
        }
    } else if (addr <= 0xBFFF) {
        if (even) {
            cart_.set_mirror_mode((val & 1) ? MirrorMode::Horizontal : MirrorMode::Vertical);
        }
        // odd -> PRG-RAM protect, ignored
    } else if (addr <= 0xDFFF) {
        if (even) irq_latch_ = val;
        else      irq_reload_ = true;
    } else {
        if (even) { irq_enable_ = false; cart_.clear_irq(); }
        else      { irq_enable_ = true; }
    }
}

uint8_t Mapper4::ppu_read(uint16_t addr) {
    auto& chr = cart_.chr();
    if (chr.empty()) return 0;
    int slot = (addr >> 10) & 0x07;
    size_t base = chr_window(slot);
    size_t off  = addr & 0x03FF;
    return chr[(base + off) % chr.size()];
}

void Mapper4::ppu_write(uint16_t addr, uint8_t val) {
    if (!cart_.chr_is_ram()) return;
    auto& chr = cart_.chr();
    int slot = (addr >> 10) & 0x07;
    size_t base = chr_window(slot);
    size_t off  = addr & 0x03FF;
    if (!chr.empty()) chr[(base + off) % chr.size()] = val;
}

void Mapper4::scanline_tick() {
    if (irq_counter_ == 0 || irq_reload_) {
        irq_counter_ = irq_latch_;
        irq_reload_ = false;
    } else {
        --irq_counter_;
    }
    if (irq_counter_ == 0 && irq_enable_) {
        cart_.raise_irq();
    }
}

} // namespace fcemu
