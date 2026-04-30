# MOD-CPU: 6502 CPU 模拟器

## 元数据 (Metadata)

- **ID**: MOD-CPU
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-001
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 功能职责 (Responsibilities)

模拟 FC/NES 使用的 Ricoh 2A03/2A07（基于 6502）CPU。

核心职责：
1. 实现所有 6502 官方指令（56 条）的译码与执行
2. 支持 13 种寻址模式
3. 模拟 STATUS 寄存器（N/V/Z/C 标志位）
4. 处理中断（NMI/IRQ/BRK/RESET）
5. 管理栈操作（PHP/PLP/PHA/PLA/JSR/RTS/RTI）
6. 实现 DMA（通过 $4014）
7. 模拟未记录操作码（可选）

## 接口设计 (Interface Design)

```cpp
// include/fcemu/cpu.h
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace fcemu {

// 6502 寄存器
struct Registers {
    uint8_t a;      // 累加器
    uint8_t x;      // X 索引寄存器
    uint8_t y;      // Y 索引寄存器
    uint8_t sp;     // 栈指针（实际地址 = 0x100 + sp）
    uint16_t pc;    // 程序计数器
    uint8_t status; // 状态寄存器（N/V/_/B/_/D/I/Z/C）
};

// 中断类型
enum class InterruptType {
    None,
    NMI,
    IRQ,
    BRK,
    RESET
};

// 内存读写回调
using ReadCallback = std::function<uint8_t(uint16_t addr)>;
using WriteCallback = std::function<void(uint16_t addr, uint8_t val)>;

class Cpu6502 {
public:
    Cpu6502();
    void reset();
    void set_callbacks(ReadCallback read, WriteCallback write);
    void set_nmi_line(bool level);
    void set_irq_line(bool level);

    // 执行一条指令，返回消耗的 CPU 周期数
    int step();

    // 执行到下一个帧（用于同步）
    int step_frame(int target_cycles);

    // 触发 DMA（来自 $4014）
    void trigger_dma(uint8_t page);

    // 寄存器访问（用于调试）
    const Registers& registers() const { return regs_; }
    void set_registers(const Registers& r) { regs_ = r; }

    // 中断查询（由 PPU/APU 调用）
    void signal_nmi();
    void signal_irq();

private:
    Registers regs_;
    ReadCallback read_;
    WriteCallback write_;
    bool nmi_pending_;
    bool irq_pending_;
    int cycles_;           // 周期计数器

    // 指令执行
    int execute_instruction(uint8_t opcode);
    void set_flag(uint8_t flag, bool value);
    bool get_flag(uint8_t flag) const;

    // 寻址模式
    uint16_t addr_immediate();
    uint16_t addr_zero_page();
    uint16_t addr_zero_page_x();
    uint16_t addr_zero_page_y();
    uint16_t addr_absolute();
    uint16_t addr_absolute_x(bool& cross_page);
    uint16_t addr_absolute_y(bool& cross_page);
    uint16_t addr_indirect_x();
    uint16_t addr_indirect_y(bool& cross_page);
    uint16_t addr_relative();
    uint16_t addr_indirect();

    // 指令实现
    int op_adc(uint16_t addr);
    int op_and(uint16_t addr);
    int op_asl(uint16_t addr, bool accum);
    // ... 其他指令
};

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-MEMORY | CPU 通过内存回调读写地址空间 |
| MOD-PPU | PPU 通过 $2000-$2007 被 CPU 访问；触发 NMI |
| MOD-APU | APU 通过 $4000-$4017 被 CPU 访问；触发 IRQ |
| MOD-CARTRIDGE | 卡带 Mapper 通过 $4020-$FFFF 被 CPU 访问 |

## 数据结构 (Data Structures)

### 指令描述符

```cpp
struct Instruction {
    uint8_t opcode;
    const char* mnemonic;
    int (Cpu6502::*handler)(uint16_t addr);
    uint16_t (Cpu6502::*address_mode)();
    int base_cycles;
    bool modifies_pc;  // 是否修改 PC（用于分支/跳转）
};
```

### 状态标志位

```cpp
constexpr uint8_t FLAG_C = 0x01;  // Carry
constexpr uint8_t FLAG_Z = 0x02;  // Zero
constexpr uint8_t FLAG_I = 0x04;  // Interrupt Disable
constexpr uint8_t FLAG_D = 0x08;  // Decimal Mode (unused in NES)
constexpr uint8_t FLAG_B = 0x10;  // Break
constexpr uint8_t FLAG_V = 0x40;  // Overflow
constexpr uint8_t FLAG_N = 0x80;  // Negative
```

### 中断向量

```cpp
constexpr uint16_t VECTOR_NMI   = 0xFFFA;
constexpr uint16_t VECTOR_RESET = 0xFFFC;
constexpr uint16_t VECTOR_IRQ   = 0xFFFE;
```

## 状态机 (State Machines)

### CPU 主循环

```
[Fetch Opcode] → [Decode] → [Addressing Mode] → [Execute] → [Check Interrupts] → repeat
```

### 中断处理

```
[Instruction Complete] → [NMI Pending?] → Yes → [Save PC+Status, Jump to NMI Vector]
                        ↓ No
                  [IRQ Enabled? & IRQ Pending?] → Yes → [Save PC+Status, Jump to IRQ Vector]
                                            ↓ No
                                       [Next Instruction]
```

### DMA 时序

```
[Write $4014] → [CPU Halt] → [Read 256 bytes from PRG ROM/RAM] → [Write to OAM] → [Resume]
(513 CPU cycles total)
```

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/cpu/instruction-set.md` - 完整指令集参考
- `docs/hardware/cpu/addressing-modes.md` - 13 种寻址模式
- `docs/hardware/cpu/registers.md` - 寄存器参考

## 变更记录 (Change History)

- 2026-04-30: Initial version
