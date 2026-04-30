// cpu.cpp - 6502 CPU emulator (partial implementation)
#include "fcemu/cpu.h"
#include <cstring>
#include <cstdio>

namespace fcemu {

static const int CYCLES_TABLE[256] = {
    // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
    7,  6,  2,  8,  5,  3,  5,  5,  3,  2,  2,  2,  6,  4,  6,  5,  // 0x00-0x0F
    3,  2,  2,  2,  4,  3,  5,  5,  2,  4,  2,  2,  6,  4,  6,  5,  // 0x10-0x1F
    2,  6,  2,  8,  5,  3,  5,  5,  4,  2,  2,  2,  6,  4,  6,  5,  // 0x20-0x2F
    4,  2,  2,  2,  6,  3,  5,  5,  2,  4,  2,  2,  6,  4,  6,  5,  // 0x30-0x3F
    6,  6,  4,  8,  7,  3,  5,  5,  3,  2,  2,  2,  6,  4,  6,  5,  // 0x40-0x4F
    4,  2,  2,  2,  6,  3,  5,  5,  2,  4,  2,  2,  6,  4,  6,  5,  // 0x50-0x5F
    2,  6,  2,  8,  5,  3,  5,  5,  4,  2,  2,  2,  6,  4,  6,  5,  // 0x60-0x6F
    4,  2,  2,  2,  6,  3,  5,  5,  2,  4,  2,  2,  6,  4,  6,  5,  // 0x70-0x7F
    2,  6,  2,  8,  5,  3,  5,  5,  3,  2,  2,  2,  6,  4,  6,  5,  // 0x80-0x8F
    4,  2,  2,  2,  6,  3,  5,  5,  2,  4,  2,  2,  6,  4,  6,  5,  // 0x90-0x9F
    2,  6,  2,  8,  5,  3,  5,  5,  4,  2,  2,  2,  6,  4,  6,  5,  // 0xA0-0xAF
    4,  2,  2,  2,  6,  3,  5,  5,  2,  4,  2,  2,  6,  4,  6,  5,  // 0xB0-0xBF
    2,  6,  2,  8,  5,  3,  5,  5,  4,  2,  2,  2,  6,  4,  6,  5,  // 0xC0-0xCF
    4,  2,  2,  2,  6,  3,  5,  5,  2,  4,  2,  2,  6,  4,  6,  5,  // 0xD0-0xDF
    2,  6,  2,  8,  5,  3,  5,  5,  4,  2,  2,  2,  6,  4,  6,  5,  // 0xE0-0xEF
    4,  2,  2,  2,  6,  3,  5,  5,  2,  4,  2,  2,  6,  4,  6,  5,  // 0xF0-0xFF
};

Cpu6502::Cpu6502()
    : regs_{}, read_(nullptr), write_(nullptr),
      nmi_pending_(false), irq_pending_(false), cycles_(0) {
    reset();
}

void Cpu6502::reset() {
    regs_.pc = (read_(0xFFFD) << 8) | read_(0xFFFC);
    regs_.sp = 0xFD;
    regs_.a = 0;
    regs_.x = 0;
    regs_.y = 0;
    regs_.status = 0x24;  // I=1, bit 5=1
    cycles_ = 0;
    nmi_pending_ = false;
    irq_pending_ = false;
}

void Cpu6502::set_callbacks(ReadCallback read, WriteCallback write) {
    read_ = read;
    write_ = write;
}

void Cpu6502::set_nmi_line(bool level) {
    if (level) nmi_pending_ = true;
}

void Cpu6502::set_irq_line(bool level) {
    irq_pending_ = level;
}

int Cpu6502::step() {
    // Check interrupts
    if (nmi_pending_) {
        nmi_pending_ = false;
        uint16_t pc = regs_.pc;
        write_(0x0100 + regs_.sp--, (pc >> 8) & 0xFF);
        write_(0x0100 + regs_.sp--, pc & 0xFF);
        write_(0x0100 + regs_.sp--, regs_.status);
        regs_.pc = (read_(0xFFFB) << 8) | read_(0xFFFA);
        set_flag(FLAG_I, true);
        return 7;
    }

    if (irq_pending_ && !get_flag(FLAG_I)) {
        irq_pending_ = false;
        uint16_t pc = regs_.pc;
        write_(0x0100 + regs_.sp--, (pc >> 8) & 0xFF);
        write_(0x0100 + regs_.sp--, pc & 0xFF);
        write_(0x0100 + regs_.sp--, regs_.status);
        regs_.pc = (read_(0xFFFF) << 8) | read_(0xFFFE);
        set_flag(FLAG_I, true);
        return 7;
    }

    uint8_t opcode = read_(regs_.pc++);
    int cycles = CYCLES_TABLE[opcode];
    return cycles + execute_instruction(opcode);
}

void Cpu6502::trigger_dma(uint8_t page) {
    // DMA transfers 256 bytes from page to OAM
    uint16_t addr = page << 8;
    for (int i = 0; i < 256; ++i) {
        // In real hardware, CPU is halted for 513 cycles
        // Here we just simulate the transfer
    }
    cycles_ += 513;
}

const Cpu6502::Registers& Cpu6502::registers() const { return regs_; }
void Cpu6502::set_registers(const Registers& r) { regs_ = r; }

void Cpu6502::signal_nmi() { nmi_pending_ = true; }
void Cpu6502::signal_irq() { irq_pending_ = true; }

// Flag helpers
void Cpu6502::set_flag(uint8_t flag, bool value) {
    if (value) regs_.status |= flag;
    else regs_.status &= ~flag;
}

bool Cpu6502::get_flag(uint8_t flag) const { return (regs_.status & flag) != 0; }

// Addressing modes (partial)
uint16_t Cpu6502::addr_immediate() { return regs_.pc++; }
uint16_t Cpu6502::addr_zero_page() { return read_(regs_.pc++); }

uint16_t Cpu6502::addr_absolute() {
    uint8_t lo = read_(regs_.pc++);
    uint8_t hi = read_(regs_.pc++);
    return (hi << 8) | lo;
}

// Instruction implementations (partial)
int Cpu6502::execute_instruction(uint8_t opcode) {
    switch (opcode) {
        case 0xA9: { // LDA immediate
            uint8_t val = read_(regs_.pc++);
            regs_.a = val;
            set_flag(FLAG_Z, val == 0);
            set_flag(FLAG_N, (val & 0x80) != 0);
            return 0;
        }
        case 0xA5: { // LDA zero page
            uint16_t addr = addr_zero_page();
            regs_.a = read_(addr);
            set_flag(FLAG_Z, regs_.a == 0);
            set_flag(FLAG_N, (regs_.a & 0x80) != 0);
            return 0;
        }
        case 0xAD: { // LDA absolute
            uint16_t addr = addr_absolute();
            regs_.a = read_(addr);
            set_flag(FLAG_Z, regs_.a == 0);
            set_flag(FLAG_N, (regs_.a & 0x80) != 0);
            return 0;
        }
        case 0x8D: { // STA absolute
            uint16_t addr = addr_absolute();
            write_(addr, regs_.a);
            return 0;
        }
        case 0x00: { // BRK
            regs_.pc++;  // Skip next byte (padding)
            uint16_t pc = regs_.pc;
            write_(0x0100 + regs_.sp--, (pc >> 8) & 0xFF);
            write_(0x0100 + regs_.sp--, pc & 0xFF);
            write_(0x0100 + regs_.sp--, regs_.status | FLAG_B);
            regs_.pc = (read_(0xFFFF) << 8) | read_(0xFFFE);
            set_flag(FLAG_I, true);
            return 0;
        }
        default:
            // Unsupported opcode
            return 0;
    }
}

} // namespace fcemu
