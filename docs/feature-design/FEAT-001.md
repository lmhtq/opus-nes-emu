# FEAT-001: CPU 指令执行

## 元数据 (Metadata)

- **ID**: FEAT-001
- **关联模块 (Related Module)**: MOD-CPU
- **关联需求 (Related Requirements)**: REQ-001
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30

## 功能描述 (Feature Description)

实现 6502 CPU 指令的执行引擎，支持所有 56 条官方指令和 13 种寻址模式。

## 接口定义 (Interface Definition)

```cpp
// 指令处理函数类型
using InstructionHandler = int (Cpu6502::*)(uint16_t addr);
using AddressMode = uint16_t (Cpu6502::*)();

// 指令表（256 项，含未记录操作码）
struct InstructionTable {
    InstructionHandler handler;
    AddressMode address_mode;
    int cycles;
    bool modifies_pc;
};

class Cpu6502 {
public:
    int step();  // 执行一条指令，返回周期数
private:
    static const InstructionTable INSTRUCTION_TABLE[256];
    int execute_instruction(uint8_t opcode);
};
```

## 流程图 (Flow Chart)

```
[Fetch Opcode from PC]
    → [PC += 1]
        → [Lookup INSTRUCTION_TABLE[opcode]]
            → [Decode: handler, address_mode, cycles]
                → [Resolve address using address_mode()]
                    → [Execute using handler(address)]
                        → [Advance time by cycles]
                            → [Check & trigger interrupts]
```

## 边界条件 (Edge Cases)

1. **跨页边界**：Absolute,X / Absolute,Y / Indirect,Y 跨页时 +1 周期
2. **BRK 指令**：$2002 bit 4（B flag）= 1，IRQ 时 = 0
3. **未记录操作码**：如 $1A/$3A/$5A/$7A/$DA/$FA 作为 NOP 处理
4. **RESET 向量**：从 $FFFC/$FFFD 读取，忽略所有标志
5. **NMI 在指令边界触发**：不在指令中间触发
6. **DEC/INC 零页变址**：地址回绕（$FF + 1 = $00）

## 测试场景 (Test Scenarios)

1. **ADC 正常加法**：A=$10, mem=$20, C=0 → A=$30, C=0, Z=0, N=0
2. **ADC 带进位**：A=$FF, mem=$01, C=1 → A=$01, C=1, Z=0, N=0
3. **ADC 溢出**：A=$7F, mem=$01, C=0 → A=$80, V=1, N=1
4. **SBC 正常减法**：A=$30, mem=$10, C=1 → A=$20, C=1
5. **分支跳转**：BCC 当 C=0 时跳转到目标地址
6. **跨页分支**：BEQ 目标地址跨页，额外 +1 周期
7. **BRK 指令**：执行后 PC 被压栈，跳转到 $FFFE/$FFFF
8. **NMI 触发**：PPU 触发 VBlank，CPU 跳转到 $FFFA/$FFFB
9. **DMA 传输**：写入 $4014，CPU 暂停 513 个周期
10. **未记录 NOP**：执行 $1A，PC 正确 +1，无副作用

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/cpu/instruction-set.md`
- `docs/hardware/cpu/addressing-modes.md`
- `docs/hardware/cpu/registers.md`

## 变更记录 (Change History)

- 2026-04-30: Initial version
