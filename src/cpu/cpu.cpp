// cpu.cpp - 6502 CPU emulator (complete implementation)
#include "fcemu/cpu.h"
#include <cstring>
#include <cstdio>

namespace fcemu {

static const int CYCLES_TABLE[256] = {
    // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
    7,  6,  2,  8,  5,  3,  5,  5,  3,  2,  2,  2,  6,  4,  6,  5,  // 0x00-0x0F
    3,  5,  2,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  6,  // 0x10-0x1F
    6,  6,  2,  8,  3,  3,  5,  5,  4,  2,  2,  2,  4,  4,  6,  6,  // 0x20-0x2F
    2,  5,  2,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  6,  // 0x30-0x3F
    6,  6,  2,  8,  3,  3,  5,  5,  3,  2,  2,  2,  3,  4,  6,  6,  // 0x40-0x4F
    2,  5,  2,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  6,  // 0x50-0x5F
    6,  6,  2,  8,  3,  3,  5,  5,  4,  2,  2,  2,  5,  4,  6,  6,  // 0x60-0x6F
    2,  5,  2,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  6,  // 0x70-0x7F
    2,  6,  2,  6,  3,  3,  3,  3,  2,  2,  2,  2,  4,  4,  4,  4,  // 0x80-0x8F
    2,  6,  2,  6,  4,  4,  4,  4,  2,  5,  2,  5,  5,  5,  5,  5,  // 0x90-0x9F
    2,  6,  2,  6,  3,  3,  3,  3,  2,  2,  2,  2,  4,  4,  4,  4,  // 0xA0-0xAF
    2,  5,  2,  5,  4,  4,  4,  4,  2,  4,  2,  4,  4,  4,  4,  4,  // 0xB0-0xBF
    2,  6,  2,  8,  3,  3,  5,  5,  2,  2,  2,  2,  4,  4,  6,  6,  // 0xC0-0xCF
    2,  5,  2,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  6,  // 0xD0-0xDF
    2,  6,  2,  8,  3,  3,  5,  5,  2,  2,  2,  2,  4,  4,  6,  6,  // 0xE0-0xEF
    2,  5,  2,  8,  4,  4,  6,  6,  2,  4,  2,  7,  4,  4,  7,  6,  // 0xF0-0xFF
};

Cpu6502::Cpu6502()
    : regs_{}, read_(nullptr), write_(nullptr),
      nmi_pending_(false), irq_pending_(false), cycles_(0) {
    regs_.sp = 0xFD;
    regs_.a = 0;
    regs_.x = 0;
    regs_.y = 0;
    regs_.status = 0x24;
    regs_.pc = 0;
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
        write_(0x0100 + regs_.sp--, regs_.status & ~FLAG_B);
        regs_.pc = (read_(0xFFFB) << 8) | read_(0xFFFA);
        set_flag(FLAG_I, true);
        return 7;
    }

    if (irq_pending_ && !get_flag(FLAG_I)) {
        irq_pending_ = false;
        uint16_t pc = regs_.pc;
        write_(0x0100 + regs_.sp--, (pc >> 8) & 0xFF);
        write_(0x0100 + regs_.sp--, pc & 0xFF);
        write_(0x0100 + regs_.sp--, regs_.status & ~FLAG_B);
        regs_.pc = (read_(0xFFFF) << 8) | read_(0xFFFE);
        set_flag(FLAG_I, true);
        return 7;
    }

    uint8_t opcode = read_(regs_.pc++);
    int cycles = CYCLES_TABLE[opcode];
    return cycles + execute_instruction(opcode);
}

int Cpu6502::step_frame(int target_cycles) {
    int total = 0;
    while (total < target_cycles) {
        total += step();
    }
    return total;
}

void Cpu6502::trigger_dma(uint8_t page) {
    uint16_t addr = page << 8;
    for (int i = 0; i < 256; ++i) {
        (void)read_(addr + i);
    }
    cycles_ += 513;
}

// Flag helpers
void Cpu6502::set_flag(uint8_t flag, bool value) {
    if (value) regs_.status |= flag;
    else regs_.status &= ~flag;
}

bool Cpu6502::get_flag(uint8_t flag) const { return (regs_.status & flag) != 0; }

// ==================== Addressing modes ====================

uint16_t Cpu6502::addr_immediate() { return regs_.pc++; }

uint16_t Cpu6502::addr_zero_page() { return read_(regs_.pc++); }

uint16_t Cpu6502::addr_zero_page_x() {
    uint8_t base = read_(regs_.pc++);
    return (base + regs_.x) & 0xFF;
}

uint16_t Cpu6502::addr_absolute() {
    uint8_t lo = read_(regs_.pc++);
    uint8_t hi = read_(regs_.pc++);
    return (hi << 8) | lo;
}

uint16_t Cpu6502::addr_absolute_x(bool& cross) {
    uint8_t lo = read_(regs_.pc++);
    uint8_t hi = read_(regs_.pc++);
    uint16_t addr = (hi << 8) | lo;
    uint16_t result = addr + regs_.x;
    cross = (addr & 0xFF00) != (result & 0xFF00);
    return result;
}

uint16_t Cpu6502::addr_absolute_y(bool& cross) {
    uint8_t lo = read_(regs_.pc++);
    uint8_t hi = read_(regs_.pc++);
    uint16_t addr = (hi << 8) | lo;
    uint16_t result = addr + regs_.y;
    cross = (addr & 0xFF00) != (result & 0xFF00);
    return result;
}

uint16_t Cpu6502::addr_indirect_x() {
    uint8_t base = read_(regs_.pc++);
    uint8_t ptr = (base + regs_.x) & 0xFF;
    uint8_t lo = read_(ptr);
    uint8_t hi = read_((ptr + 1) & 0xFF);
    return (hi << 8) | lo;
}

uint16_t Cpu6502::addr_indirect_y(bool& cross) {
    uint8_t base = read_(regs_.pc++);
    uint8_t lo = read_(base);
    uint8_t hi = read_((base + 1) & 0xFF);
    uint16_t addr = (hi << 8) | lo;
    uint16_t result = addr + regs_.y;
    cross = (addr & 0xFF00) != (result & 0xFF00);
    return result;
}

uint16_t Cpu6502::addr_relative() {
    int8_t offset = static_cast<int8_t>(read_(regs_.pc++));
    return regs_.pc + offset;
}

// ==================== Instruction execution ====================

int Cpu6502::execute_instruction(uint8_t opcode) {
    switch (opcode) {

    // ==================== LDA ====================
    case 0xA9: { // LDA immediate
        regs_.a = read_(regs_.pc++);
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0xA5: { // LDA zero page
        regs_.a = read_(addr_zero_page());
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0xB5: { // LDA zero page,X
        regs_.a = read_(addr_zero_page_x());
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0xAD: { // LDA absolute
        regs_.a = read_(addr_absolute());
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0xBD: { // LDA absolute,X
        bool cross = false;
        regs_.a = read_(addr_absolute_x(cross));
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return cross ? 1 : 0;
    }
    case 0xB9: { // LDA absolute,Y
        bool cross = false;
        regs_.a = read_(addr_absolute_y(cross));
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return cross ? 1 : 0;
    }
    case 0xA1: { // LDA (indirect,X)
        regs_.a = read_(addr_indirect_x());
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0xB1: { // LDA (indirect),Y
        bool cross = false;
        regs_.a = read_(addr_indirect_y(cross));
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return cross ? 1 : 0;
    }

    // ==================== LDX ====================
    case 0xA2: { // LDX immediate
        regs_.x = read_(regs_.pc++);
        set_flag(FLAG_Z, regs_.x == 0);
        set_flag(FLAG_N, (regs_.x & 0x80) != 0);
        return 0;
    }
    case 0xA6: { // LDX zero page
        regs_.x = read_(addr_zero_page());
        set_flag(FLAG_Z, regs_.x == 0);
        set_flag(FLAG_N, (regs_.x & 0x80) != 0);
        return 0;
    }
    case 0xB6: { // LDX zero page,Y
        uint8_t base = read_(regs_.pc++);
        uint16_t addr = (base + regs_.y) & 0xFF;
        regs_.x = read_(addr);
        set_flag(FLAG_Z, regs_.x == 0);
        set_flag(FLAG_N, (regs_.x & 0x80) != 0);
        return 0;
    }
    case 0xAE: { // LDX absolute
        regs_.x = read_(addr_absolute());
        set_flag(FLAG_Z, regs_.x == 0);
        set_flag(FLAG_N, (regs_.x & 0x80) != 0);
        return 0;
    }
    case 0xBE: { // LDX absolute,Y
        bool cross = false;
        regs_.x = read_(addr_absolute_y(cross));
        set_flag(FLAG_Z, regs_.x == 0);
        set_flag(FLAG_N, (regs_.x & 0x80) != 0);
        return cross ? 1 : 0;
    }

    // ==================== LDY ====================
    case 0xA0: { // LDY immediate
        regs_.y = read_(regs_.pc++);
        set_flag(FLAG_Z, regs_.y == 0);
        set_flag(FLAG_N, (regs_.y & 0x80) != 0);
        return 0;
    }
    case 0xA4: { // LDY zero page
        regs_.y = read_(addr_zero_page());
        set_flag(FLAG_Z, regs_.y == 0);
        set_flag(FLAG_N, (regs_.y & 0x80) != 0);
        return 0;
    }
    case 0xB4: { // LDY zero page,X
        regs_.y = read_(addr_zero_page_x());
        set_flag(FLAG_Z, regs_.y == 0);
        set_flag(FLAG_N, (regs_.y & 0x80) != 0);
        return 0;
    }
    case 0xAC: { // LDY absolute
        regs_.y = read_(addr_absolute());
        set_flag(FLAG_Z, regs_.y == 0);
        set_flag(FLAG_N, (regs_.y & 0x80) != 0);
        return 0;
    }
    case 0xBC: { // LDY absolute,X
        bool cross = false;
        regs_.y = read_(addr_absolute_x(cross));
        set_flag(FLAG_Z, regs_.y == 0);
        set_flag(FLAG_N, (regs_.y & 0x80) != 0);
        return cross ? 1 : 0;
    }

    // ==================== STA ====================
    case 0x85: { write_(addr_zero_page(), regs_.a); return 0; }      // STA zero page
    case 0x95: { write_(addr_zero_page_x(), regs_.a); return 0; }    // STA zero page,X
    case 0x8D: { write_(addr_absolute(), regs_.a); return 0; }       // STA absolute
    case 0x9D: { // STA absolute,X
        bool cross = false;
        write_(addr_absolute_x(cross), regs_.a);
        return 0;
    }
    case 0x99: { // STA absolute,Y
        bool cross = false;
        write_(addr_absolute_y(cross), regs_.a);
        return 0;
    }
    case 0x81: { write_(addr_indirect_x(), regs_.a); return 0; }     // STA (indirect,X)
    case 0x91: { // STA (indirect),Y
        bool cross = false;
        write_(addr_indirect_y(cross), regs_.a);
        return 0;
    }

    // ==================== STX ====================
    case 0x86: { write_(addr_zero_page(), regs_.x); return 0; }      // STX zero page
    case 0x96: { // STX zero page,Y
        uint8_t base = read_(regs_.pc++);
        write_((base + regs_.y) & 0xFF, regs_.x);
        return 0;
    }
    case 0x8E: { write_(addr_absolute(), regs_.x); return 0; }       // STX absolute

    // ==================== STY ====================
    case 0x84: { write_(addr_zero_page(), regs_.y); return 0; }      // STY zero page
    case 0x94: { write_(addr_zero_page_x(), regs_.y); return 0; }    // STY zero page,X
    case 0x8C: { write_(addr_absolute(), regs_.y); return 0; }       // STY absolute

    // ==================== ADC ====================
    case 0x69: { // ADC immediate
        uint8_t val = read_(regs_.pc++);
        uint16_t result = regs_.a + val + (get_flag(FLAG_C) ? 1 : 0);
        set_flag(FLAG_C, result > 0xFF);
        set_flag(FLAG_V, (~(regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0x65: { // ADC zero page
        uint8_t val = read_(addr_zero_page());
        uint16_t result = regs_.a + val + (get_flag(FLAG_C) ? 1 : 0);
        set_flag(FLAG_C, result > 0xFF);
        set_flag(FLAG_V, (~(regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0x75: { // ADC zero page,X
        uint8_t val = read_(addr_zero_page_x());
        uint16_t result = regs_.a + val + (get_flag(FLAG_C) ? 1 : 0);
        set_flag(FLAG_C, result > 0xFF);
        set_flag(FLAG_V, (~(regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0x6D: { // ADC absolute
        uint8_t val = read_(addr_absolute());
        uint16_t result = regs_.a + val + (get_flag(FLAG_C) ? 1 : 0);
        set_flag(FLAG_C, result > 0xFF);
        set_flag(FLAG_V, (~(regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0x7D: { // ADC absolute,X
        bool cross = false;
        uint8_t val = read_(addr_absolute_x(cross));
        uint16_t result = regs_.a + val + (get_flag(FLAG_C) ? 1 : 0);
        set_flag(FLAG_C, result > 0xFF);
        set_flag(FLAG_V, (~(regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return cross ? 1 : 0;
    }
    case 0x79: { // ADC absolute,Y
        bool cross = false;
        uint8_t val = read_(addr_absolute_y(cross));
        uint16_t result = regs_.a + val + (get_flag(FLAG_C) ? 1 : 0);
        set_flag(FLAG_C, result > 0xFF);
        set_flag(FLAG_V, (~(regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return cross ? 1 : 0;
    }
    case 0x61: { // ADC (indirect,X)
        uint8_t val = read_(addr_indirect_x());
        uint16_t result = regs_.a + val + (get_flag(FLAG_C) ? 1 : 0);
        set_flag(FLAG_C, result > 0xFF);
        set_flag(FLAG_V, (~(regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0x71: { // ADC (indirect),Y
        bool cross = false;
        uint8_t val = read_(addr_indirect_y(cross));
        uint16_t result = regs_.a + val + (get_flag(FLAG_C) ? 1 : 0);
        set_flag(FLAG_C, result > 0xFF);
        set_flag(FLAG_V, (~(regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return cross ? 1 : 0;
    }

    // ==================== SBC ====================
    case 0xE9: { // SBC immediate
        uint8_t val = read_(regs_.pc++);
        uint16_t result = regs_.a - val - (get_flag(FLAG_C) ? 0 : 1);
        set_flag(FLAG_C, result <= 0xFF);
        set_flag(FLAG_V, ((regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0xE5: { // SBC zero page
        uint8_t val = read_(addr_zero_page());
        uint16_t result = regs_.a - val - (get_flag(FLAG_C) ? 0 : 1);
        set_flag(FLAG_C, result <= 0xFF);
        set_flag(FLAG_V, ((regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0xF5: { // SBC zero page,X
        uint8_t val = read_(addr_zero_page_x());
        uint16_t result = regs_.a - val - (get_flag(FLAG_C) ? 0 : 1);
        set_flag(FLAG_C, result <= 0xFF);
        set_flag(FLAG_V, ((regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0xED: { // SBC absolute
        uint8_t val = read_(addr_absolute());
        uint16_t result = regs_.a - val - (get_flag(FLAG_C) ? 0 : 1);
        set_flag(FLAG_C, result <= 0xFF);
        set_flag(FLAG_V, ((regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0xFD: { // SBC absolute,X
        bool cross = false;
        uint8_t val = read_(addr_absolute_x(cross));
        uint16_t result = regs_.a - val - (get_flag(FLAG_C) ? 0 : 1);
        set_flag(FLAG_C, result <= 0xFF);
        set_flag(FLAG_V, ((regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return cross ? 1 : 0;
    }
    case 0xF9: { // SBC absolute,Y
        bool cross = false;
        uint8_t val = read_(addr_absolute_y(cross));
        uint16_t result = regs_.a - val - (get_flag(FLAG_C) ? 0 : 1);
        set_flag(FLAG_C, result <= 0xFF);
        set_flag(FLAG_V, ((regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return cross ? 1 : 0;
    }
    case 0xE1: { // SBC (indirect,X)
        uint8_t val = read_(addr_indirect_x());
        uint16_t result = regs_.a - val - (get_flag(FLAG_C) ? 0 : 1);
        set_flag(FLAG_C, result <= 0xFF);
        set_flag(FLAG_V, ((regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0xF1: { // SBC (indirect),Y
        bool cross = false;
        uint8_t val = read_(addr_indirect_y(cross));
        uint16_t result = regs_.a - val - (get_flag(FLAG_C) ? 0 : 1);
        set_flag(FLAG_C, result <= 0xFF);
        set_flag(FLAG_V, ((regs_.a ^ val) & (regs_.a ^ result) & 0x80) != 0);
        regs_.a = result & 0xFF;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return cross ? 1 : 0;
    }

    // ==================== AND ====================
    case 0x29: { regs_.a &= read_(regs_.pc++); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x25: { regs_.a &= read_(addr_zero_page()); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x35: { regs_.a &= read_(addr_zero_page_x()); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x2D: { regs_.a &= read_(addr_absolute()); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x3D: { bool c = false; regs_.a &= read_(addr_absolute_x(c)); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return c ? 1 : 0; }
    case 0x39: { bool c = false; regs_.a &= read_(addr_absolute_y(c)); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return c ? 1 : 0; }
    case 0x21: { regs_.a &= read_(addr_indirect_x()); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x31: { bool c = false; regs_.a &= read_(addr_indirect_y(c)); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return c ? 1 : 0; }

    // ==================== ORA ====================
    case 0x09: { regs_.a |= read_(regs_.pc++); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x05: { regs_.a |= read_(addr_zero_page()); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x15: { regs_.a |= read_(addr_zero_page_x()); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x0D: { regs_.a |= read_(addr_absolute()); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x1D: { bool c = false; regs_.a |= read_(addr_absolute_x(c)); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return c ? 1 : 0; }
    case 0x19: { bool c = false; regs_.a |= read_(addr_absolute_y(c)); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return c ? 1 : 0; }
    case 0x01: { regs_.a |= read_(addr_indirect_x()); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x11: { bool c = false; regs_.a |= read_(addr_indirect_y(c)); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return c ? 1 : 0; }

    // ==================== EOR ====================
    case 0x49: { regs_.a ^= read_(regs_.pc++); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x45: { regs_.a ^= read_(addr_zero_page()); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x55: { regs_.a ^= read_(addr_zero_page_x()); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x4D: { regs_.a ^= read_(addr_absolute()); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x5D: { bool c = false; regs_.a ^= read_(addr_absolute_x(c)); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return c ? 1 : 0; }
    case 0x59: { bool c = false; regs_.a ^= read_(addr_absolute_y(c)); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return c ? 1 : 0; }
    case 0x41: { regs_.a ^= read_(addr_indirect_x()); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; }
    case 0x51: { bool c = false; regs_.a ^= read_(addr_indirect_y(c)); set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return c ? 1 : 0; }

    // ==================== CMP ====================
    case 0xC9: { uint8_t v = read_(regs_.pc++); set_flag(FLAG_C, regs_.a >= v); set_flag(FLAG_Z, regs_.a == v); set_flag(FLAG_N, ((regs_.a - v) & 0x80) != 0); return 0; }
    case 0xC5: { uint8_t v = read_(addr_zero_page()); set_flag(FLAG_C, regs_.a >= v); set_flag(FLAG_Z, regs_.a == v); set_flag(FLAG_N, ((regs_.a - v) & 0x80) != 0); return 0; }
    case 0xD5: { uint8_t v = read_(addr_zero_page_x()); set_flag(FLAG_C, regs_.a >= v); set_flag(FLAG_Z, regs_.a == v); set_flag(FLAG_N, ((regs_.a - v) & 0x80) != 0); return 0; }
    case 0xCD: { uint8_t v = read_(addr_absolute()); set_flag(FLAG_C, regs_.a >= v); set_flag(FLAG_Z, regs_.a == v); set_flag(FLAG_N, ((regs_.a - v) & 0x80) != 0); return 0; }
    case 0xDD: { bool c = false; uint8_t v = read_(addr_absolute_x(c)); set_flag(FLAG_C, regs_.a >= v); set_flag(FLAG_Z, regs_.a == v); set_flag(FLAG_N, ((regs_.a - v) & 0x80) != 0); return c ? 1 : 0; }
    case 0xD9: { bool c = false; uint8_t v = read_(addr_absolute_y(c)); set_flag(FLAG_C, regs_.a >= v); set_flag(FLAG_Z, regs_.a == v); set_flag(FLAG_N, ((regs_.a - v) & 0x80) != 0); return c ? 1 : 0; }
    case 0xC1: { uint8_t v = read_(addr_indirect_x()); set_flag(FLAG_C, regs_.a >= v); set_flag(FLAG_Z, regs_.a == v); set_flag(FLAG_N, ((regs_.a - v) & 0x80) != 0); return 0; }
    case 0xD1: { bool c = false; uint8_t v = read_(addr_indirect_y(c)); set_flag(FLAG_C, regs_.a >= v); set_flag(FLAG_Z, regs_.a == v); set_flag(FLAG_N, ((regs_.a - v) & 0x80) != 0); return c ? 1 : 0; }

    // ==================== CPX ====================
    case 0xE0: { uint8_t v = read_(regs_.pc++); set_flag(FLAG_C, regs_.x >= v); set_flag(FLAG_Z, regs_.x == v); set_flag(FLAG_N, ((regs_.x - v) & 0x80) != 0); return 0; }
    case 0xE4: { uint8_t v = read_(addr_zero_page()); set_flag(FLAG_C, regs_.x >= v); set_flag(FLAG_Z, regs_.x == v); set_flag(FLAG_N, ((regs_.x - v) & 0x80) != 0); return 0; }
    case 0xEC: { uint8_t v = read_(addr_absolute()); set_flag(FLAG_C, regs_.x >= v); set_flag(FLAG_Z, regs_.x == v); set_flag(FLAG_N, ((regs_.x - v) & 0x80) != 0); return 0; }

    // ==================== CPY ====================
    case 0xC0: { uint8_t v = read_(regs_.pc++); set_flag(FLAG_C, regs_.y >= v); set_flag(FLAG_Z, regs_.y == v); set_flag(FLAG_N, ((regs_.y - v) & 0x80) != 0); return 0; }
    case 0xC4: { uint8_t v = read_(addr_zero_page()); set_flag(FLAG_C, regs_.y >= v); set_flag(FLAG_Z, regs_.y == v); set_flag(FLAG_N, ((regs_.y - v) & 0x80) != 0); return 0; }
    case 0xCC: { uint8_t v = read_(addr_absolute()); set_flag(FLAG_C, regs_.y >= v); set_flag(FLAG_Z, regs_.y == v); set_flag(FLAG_N, ((regs_.y - v) & 0x80) != 0); return 0; }

    // ==================== INC ====================
    case 0xE6: { uint16_t a = addr_zero_page(); uint8_t v = read_(a) + 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0xF6: { uint16_t a = addr_zero_page_x(); uint8_t v = read_(a) + 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0xEE: { uint16_t a = addr_absolute(); uint8_t v = read_(a) + 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0xFE: { bool c = false; uint16_t a = addr_absolute_x(c); uint8_t v = read_(a) + 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }

    // ==================== DEC ====================
    case 0xC6: { uint16_t a = addr_zero_page(); uint8_t v = read_(a) - 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0xD6: { uint16_t a = addr_zero_page_x(); uint8_t v = read_(a) - 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0xCE: { uint16_t a = addr_absolute(); uint8_t v = read_(a) - 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0xDE: { bool c = false; uint16_t a = addr_absolute_x(c); uint8_t v = read_(a) - 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }

    // ==================== ASL ====================
    case 0x0A: { // ASL accumulator
        set_flag(FLAG_C, (regs_.a & 0x80) != 0);
        regs_.a <<= 1;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0x06: { uint16_t a = addr_zero_page(); uint8_t v = read_(a); set_flag(FLAG_C, (v & 0x80) != 0); v <<= 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0x16: { uint16_t a = addr_zero_page_x(); uint8_t v = read_(a); set_flag(FLAG_C, (v & 0x80) != 0); v <<= 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0x0E: { uint16_t a = addr_absolute(); uint8_t v = read_(a); set_flag(FLAG_C, (v & 0x80) != 0); v <<= 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0x1E: { bool c = false; uint16_t a = addr_absolute_x(c); uint8_t v = read_(a); set_flag(FLAG_C, (v & 0x80) != 0); v <<= 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }

    // ==================== LSR ====================
    case 0x4A: { // LSR accumulator
        set_flag(FLAG_C, (regs_.a & 0x01) != 0);
        regs_.a >>= 1;
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, false);
        return 0;
    }
    case 0x46: { uint16_t a = addr_zero_page(); uint8_t v = read_(a); set_flag(FLAG_C, (v & 0x01) != 0); v >>= 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, false); return 0; }
    case 0x56: { uint16_t a = addr_zero_page_x(); uint8_t v = read_(a); set_flag(FLAG_C, (v & 0x01) != 0); v >>= 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, false); return 0; }
    case 0x4E: { uint16_t a = addr_absolute(); uint8_t v = read_(a); set_flag(FLAG_C, (v & 0x01) != 0); v >>= 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, false); return 0; }
    case 0x5E: { bool c = false; uint16_t a = addr_absolute_x(c); uint8_t v = read_(a); set_flag(FLAG_C, (v & 0x01) != 0); v >>= 1; write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, false); return 0; }

    // ==================== ROL ====================
    case 0x2A: { // ROL accumulator
        bool old_carry = get_flag(FLAG_C);
        set_flag(FLAG_C, (regs_.a & 0x80) != 0);
        regs_.a = (regs_.a << 1) | (old_carry ? 1 : 0);
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0x26: { uint16_t a = addr_zero_page(); uint8_t v = read_(a); bool oc = get_flag(FLAG_C); set_flag(FLAG_C, (v & 0x80) != 0); v = (v << 1) | (oc ? 1 : 0); write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0x36: { uint16_t a = addr_zero_page_x(); uint8_t v = read_(a); bool oc = get_flag(FLAG_C); set_flag(FLAG_C, (v & 0x80) != 0); v = (v << 1) | (oc ? 1 : 0); write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0x2E: { uint16_t a = addr_absolute(); uint8_t v = read_(a); bool oc = get_flag(FLAG_C); set_flag(FLAG_C, (v & 0x80) != 0); v = (v << 1) | (oc ? 1 : 0); write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0x3E: { bool c = false; uint16_t a = addr_absolute_x(c); uint8_t v = read_(a); bool oc = get_flag(FLAG_C); set_flag(FLAG_C, (v & 0x80) != 0); v = (v << 1) | (oc ? 1 : 0); write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }

    // ==================== ROR ====================
    case 0x6A: { // ROR accumulator
        bool old_carry = get_flag(FLAG_C);
        set_flag(FLAG_C, (regs_.a & 0x01) != 0);
        regs_.a = (regs_.a >> 1) | (old_carry ? 0x80 : 0);
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0x66: { uint16_t a = addr_zero_page(); uint8_t v = read_(a); bool oc = get_flag(FLAG_C); set_flag(FLAG_C, (v & 0x01) != 0); v = (v >> 1) | (oc ? 0x80 : 0); write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0x76: { uint16_t a = addr_zero_page_x(); uint8_t v = read_(a); bool oc = get_flag(FLAG_C); set_flag(FLAG_C, (v & 0x01) != 0); v = (v >> 1) | (oc ? 0x80 : 0); write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0x6E: { uint16_t a = addr_absolute(); uint8_t v = read_(a); bool oc = get_flag(FLAG_C); set_flag(FLAG_C, (v & 0x01) != 0); v = (v >> 1) | (oc ? 0x80 : 0); write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }
    case 0x7E: { bool c = false; uint16_t a = addr_absolute_x(c); uint8_t v = read_(a); bool oc = get_flag(FLAG_C); set_flag(FLAG_C, (v & 0x01) != 0); v = (v >> 1) | (oc ? 0x80 : 0); write_(a, v); set_flag(FLAG_Z, v == 0); set_flag(FLAG_N, (v & 0x80) != 0); return 0; }

    // ==================== Branches ====================
    case 0x10: { // BPL
        uint16_t addr = addr_relative();
        if (!get_flag(FLAG_N)) { regs_.pc = addr; return 1; }
        return 0;
    }
    case 0x30: { // BMI
        uint16_t addr = addr_relative();
        if (get_flag(FLAG_N)) { regs_.pc = addr; return 1; }
        return 0;
    }
    case 0x50: { // BVC
        uint16_t addr = addr_relative();
        if (!get_flag(FLAG_V)) { regs_.pc = addr; return 1; }
        return 0;
    }
    case 0x70: { // BVS
        uint16_t addr = addr_relative();
        if (get_flag(FLAG_V)) { regs_.pc = addr; return 1; }
        return 0;
    }
    case 0x90: { // BCC
        uint16_t addr = addr_relative();
        if (!get_flag(FLAG_C)) { regs_.pc = addr; return 1; }
        return 0;
    }
    case 0xB0: { // BCS
        uint16_t addr = addr_relative();
        if (get_flag(FLAG_C)) { regs_.pc = addr; return 1; }
        return 0;
    }
    case 0xD0: { // BNE
        uint16_t addr = addr_relative();
        if (!get_flag(FLAG_Z)) { regs_.pc = addr; return 1; }
        return 0;
    }
    case 0xF0: { // BEQ
        uint16_t addr = addr_relative();
        if (get_flag(FLAG_Z)) { regs_.pc = addr; return 1; }
        return 0;
    }

    // ==================== JMP ====================
    case 0x4C: { // JMP absolute
        uint8_t lo = read_(regs_.pc++);
        uint8_t hi = read_(regs_.pc++);
        regs_.pc = (hi << 8) | lo;
        return 0;
    }
    case 0x6C: { // JMP indirect
        uint8_t lo = read_(regs_.pc++);
        uint8_t hi = read_(regs_.pc++);
        uint16_t addr = (hi << 8) | lo;
        // 6502 bug: wraps within page
        uint8_t rlo = read_(addr);
        uint8_t rhi = read_((addr & 0xFF00) | ((addr + 1) & 0x00FF));
        regs_.pc = (rhi << 8) | rlo;
        return 0;
    }

    // ==================== JSR / RTS / RTI ====================
    case 0x20: { // JSR absolute
        uint8_t lo = read_(regs_.pc++);
        uint8_t hi = read_(regs_.pc++);
        uint16_t ret = regs_.pc - 1;
        write_(0x0100 + regs_.sp--, (ret >> 8) & 0xFF);
        write_(0x0100 + regs_.sp--, ret & 0xFF);
        regs_.pc = (hi << 8) | lo;
        return 0;
    }
    case 0x60: { // RTS
        uint8_t lo = read_(0x0100 + ++regs_.sp);
        uint8_t hi = read_(0x0100 + ++regs_.sp);
        regs_.pc = ((hi << 8) | lo) + 1;
        return 0;
    }
    case 0x40: { // RTI
        regs_.status = read_(0x0100 + ++regs_.sp);
        uint8_t lo = read_(0x0100 + ++regs_.sp);
        uint8_t hi = read_(0x0100 + ++regs_.sp);
        regs_.pc = (hi << 8) | lo;
        return 0;
    }

    // ==================== Stack ====================
    case 0x48: { // PHA
        write_(0x0100 + regs_.sp--, regs_.a);
        return 0;
    }
    case 0x08: { // PHP
        write_(0x0100 + regs_.sp--, regs_.status | FLAG_B | 0x20);
        return 0;
    }
    case 0x68: { // PLA
        regs_.a = read_(0x0100 + ++regs_.sp);
        set_flag(FLAG_Z, regs_.a == 0);
        set_flag(FLAG_N, (regs_.a & 0x80) != 0);
        return 0;
    }
    case 0x28: { // PLP
        regs_.status = (read_(0x0100 + ++regs_.sp) & ~FLAG_B) | 0x20;
        return 0;
    }

    // ==================== Transfers ====================
    case 0xAA: { regs_.x = regs_.a; set_flag(FLAG_Z, regs_.x == 0); set_flag(FLAG_N, (regs_.x & 0x80) != 0); return 0; } // TAX
    case 0xA8: { regs_.y = regs_.a; set_flag(FLAG_Z, regs_.y == 0); set_flag(FLAG_N, (regs_.y & 0x80) != 0); return 0; } // TAY
    case 0x8A: { regs_.a = regs_.x; set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; } // TXA
    case 0x98: { regs_.a = regs_.y; set_flag(FLAG_Z, regs_.a == 0); set_flag(FLAG_N, (regs_.a & 0x80) != 0); return 0; } // TYA
    case 0xBA: { regs_.x = regs_.sp; set_flag(FLAG_Z, regs_.x == 0); set_flag(FLAG_N, (regs_.x & 0x80) != 0); return 0; } // TSX
    case 0x9A: { regs_.sp = regs_.x; return 0; } // TXS

    // ==================== INX/INY/DEX/DEY ====================
    case 0xE8: { regs_.x++; set_flag(FLAG_Z, regs_.x == 0); set_flag(FLAG_N, (regs_.x & 0x80) != 0); return 0; } // INX
    case 0xC8: { regs_.y++; set_flag(FLAG_Z, regs_.y == 0); set_flag(FLAG_N, (regs_.y & 0x80) != 0); return 0; } // INY
    case 0xCA: { regs_.x--; set_flag(FLAG_Z, regs_.x == 0); set_flag(FLAG_N, (regs_.x & 0x80) != 0); return 0; } // DEX
    case 0x88: { regs_.y--; set_flag(FLAG_Z, regs_.y == 0); set_flag(FLAG_N, (regs_.y & 0x80) != 0); return 0; } // DEY

    // ==================== BIT ====================
    case 0x24: { // BIT zero page
        uint8_t v = read_(addr_zero_page());
        set_flag(FLAG_Z, (regs_.a & v) == 0);
        set_flag(FLAG_N, (v & 0x80) != 0);
        set_flag(FLAG_V, (v & 0x40) != 0);
        return 0;
    }
    case 0x2C: { // BIT absolute
        uint8_t v = read_(addr_absolute());
        set_flag(FLAG_Z, (regs_.a & v) == 0);
        set_flag(FLAG_N, (v & 0x80) != 0);
        set_flag(FLAG_V, (v & 0x40) != 0);
        return 0;
    }

    // ==================== Flag operations ====================
    case 0x18: { set_flag(FLAG_C, false); return 0; } // CLC
    case 0x38: { set_flag(FLAG_C, true); return 0; }  // SEC
    case 0x58: { set_flag(FLAG_I, false); return 0; } // CLI
    case 0x78: { set_flag(FLAG_I, true); return 0; }  // SEI
    case 0xB8: { set_flag(FLAG_V, false); return 0; } // CLV
    case 0xD8: { set_flag(FLAG_D, false); return 0; } // CLD
    case 0xF8: { set_flag(FLAG_D, true); return 0; }  // SED

    // ==================== NOP ====================
    case 0xEA: { return 0; } // NOP

    // ==================== BRK ====================
    case 0x00: {
        regs_.pc++;  // Skip padding byte
        uint16_t pc = regs_.pc;
        write_(0x0100 + regs_.sp--, (pc >> 8) & 0xFF);
        write_(0x0100 + regs_.sp--, pc & 0xFF);
        write_(0x0100 + regs_.sp--, regs_.status | FLAG_B);
        regs_.pc = (read_(0xFFFF) << 8) | read_(0xFFFE);
        set_flag(FLAG_I, true);
        return 0;
    }

    default:
        // Unsupported or illegal opcode - treat as NOP
        return 0;
    }
}

} // namespace fcemu
