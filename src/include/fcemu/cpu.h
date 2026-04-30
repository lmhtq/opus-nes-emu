// include/fcemu/cpu.h
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace fcemu {

struct Registers {
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    uint16_t pc;
    uint8_t status;
};

enum InterruptType { None, NMI, IRQ, BRK, RESET };

using ReadCallback = std::function<uint8_t(uint16_t)>;
using WriteCallback = std::function<void(uint16_t, uint8_t)>;

class Cpu6502 {
public:
    Cpu6502();
    void reset();
    void set_callbacks(ReadCallback read, WriteCallback write);
    void set_nmi_line(bool level);
    void set_irq_line(bool level);
    int step();
    int step_frame(int target_cycles);
    void trigger_dma(uint8_t page);
    const Registers& registers() const { return regs_; }
    void set_registers(const Registers& r) { regs_ = r; }
    void signal_nmi() { nmi_pending_ = true; }
    void signal_irq() { irq_pending_ = true; }

    bool get_flag(uint8_t flag) const;

    // ---- Save state -----------------------------------------------------
    void serialize(class Serializer& s) const;
    void deserialize(class Deserializer& d);

    static const uint8_t FLAG_C = 0x01;
    static const uint8_t FLAG_Z = 0x02;
    static const uint8_t FLAG_I = 0x04;
    static const uint8_t FLAG_D = 0x08;
    static const uint8_t FLAG_B = 0x10;
    static const uint8_t FLAG_V = 0x40;
    static const uint8_t FLAG_N = 0x80;

private:
    Registers regs_;
    ReadCallback read_;
    WriteCallback write_;
    bool nmi_pending_;
    bool irq_pending_;
    int cycles_;

    int execute_instruction(uint8_t opcode);
    void set_flag(uint8_t flag, bool value);

    uint16_t addr_immediate();
    uint16_t addr_zero_page();
    uint16_t addr_zero_page_x();
    uint16_t addr_absolute();
    uint16_t addr_absolute_x(bool& cross);
    uint16_t addr_absolute_y(bool& cross);
    uint16_t addr_indirect_x();
    uint16_t addr_indirect_y(bool& cross);
    uint16_t addr_relative();
};

} // namespace fcemu
