// mappers.h - Concrete iNES mapper implementations (0,1,2,3,4).
#pragma once

#include "fcemu/cartridge.h"

namespace fcemu {

// Mapper 0 (NROM): 16 KB or 32 KB PRG, 8 KB CHR, no bank switching.
class Mapper0 : public Mapper {
public:
    explicit Mapper0(Cartridge& c) : Mapper(c) {}
    int number() const override { return 0; }
    void reset() override {}
    uint8_t cpu_read(uint16_t addr) override;
    void    cpu_write(uint16_t addr, uint8_t val) override;
    uint8_t ppu_read(uint16_t addr) override;
    void    ppu_write(uint16_t addr, uint8_t val) override;
};

// Mapper 1 (MMC1): serial shift register.
class Mapper1 : public Mapper {
public:
    explicit Mapper1(Cartridge& c) : Mapper(c) {}
    int number() const override { return 1; }
    void reset() override;
    uint8_t cpu_read(uint16_t addr) override;
    void    cpu_write(uint16_t addr, uint8_t val) override;
    uint8_t ppu_read(uint16_t addr) override;
    void    ppu_write(uint16_t addr, uint8_t val) override;

private:
    uint8_t shift_   = 0x10;   // bit 4 set = empty marker
    uint8_t control_ = 0x0C;
    uint8_t chr0_    = 0;
    uint8_t chr1_    = 0;
    uint8_t prg_     = 0;
    void write_register(uint16_t addr, uint8_t val);
    void apply_mirror();
    size_t prg_offset(int slot) const;   // slot 0 = $8000, slot 1 = $C000
    size_t chr_offset(int slot) const;   // slot 0 = $0000, slot 1 = $1000
};

// Mapper 2 (UxROM): 16 KB switchable PRG at $8000, last bank fixed at $C000, CHR RAM.
class Mapper2 : public Mapper {
public:
    explicit Mapper2(Cartridge& c) : Mapper(c) {}
    int number() const override { return 2; }
    void reset() override { prg_bank_ = 0; }
    uint8_t cpu_read(uint16_t addr) override;
    void    cpu_write(uint16_t addr, uint8_t val) override;
    uint8_t ppu_read(uint16_t addr) override;
    void    ppu_write(uint16_t addr, uint8_t val) override;
private:
    uint8_t prg_bank_ = 0;
};

// Mapper 3 (CNROM): 8 KB CHR bank switching, fixed PRG.
class Mapper3 : public Mapper {
public:
    explicit Mapper3(Cartridge& c) : Mapper(c) {}
    int number() const override { return 3; }
    void reset() override { chr_bank_ = 0; }
    uint8_t cpu_read(uint16_t addr) override;
    void    cpu_write(uint16_t addr, uint8_t val) override;
    uint8_t ppu_read(uint16_t addr) override;
    void    ppu_write(uint16_t addr, uint8_t val) override;
private:
    uint8_t chr_bank_ = 0;
};

// Mapper 4 (MMC3): 8 bank registers, scanline IRQ counter.
class Mapper4 : public Mapper {
public:
    explicit Mapper4(Cartridge& c) : Mapper(c) {}
    int number() const override { return 4; }
    void reset() override;
    uint8_t cpu_read(uint16_t addr) override;
    void    cpu_write(uint16_t addr, uint8_t val) override;
    uint8_t ppu_read(uint16_t addr) override;
    void    ppu_write(uint16_t addr, uint8_t val) override;
    void    scanline_tick() override;
private:
    uint8_t bank_select_ = 0;
    uint8_t bank_regs_[8] = {0};
    uint8_t irq_latch_   = 0;
    uint8_t irq_counter_ = 0;
    bool    irq_reload_  = false;
    bool    irq_enable_  = false;
    bool    prg_mode_    = false;
    bool    chr_mode_    = false;
    size_t  prg_window(int slot) const;     // slot 0..3 = $8000,$A000,$C000,$E000
    size_t  chr_window(int slot) const;     // slot 0..7 = each 1KB chunk
};

} // namespace fcemu
