# 6502 CPU 指令集参考

FC/NES 使用 Ricoh 2A03（NTSC）或 Ricoh 2A07（PAL）处理器，基于 MOS 6502 内核，但移除了 BCD 十进制模式。

## 寄存器

| 寄存器 | 宽度 | 说明 |
|--------|------|------|
| A | 8-bit | 累加器（Accumulator） |
| X | 8-bit | X 索引寄存器 |
| Y | 8-bit | Y 索引寄存器 |
| SP | 8-bit | 栈指针（Stack Pointer），实际地址为 $0100 + SP |
| PC | 16-bit | 程序计数器（Program Counter） |
| STATUS | 8-bit | 状态寄存器（Processor Status） |

## STATUS 寄存器标志位

| 位 | 名称 | 说明 |
|----|------|------|
| 0 (C) | Carry | 进位标志 |
| 1 (Z) | Zero | 零标志（结果为0时置位） |
| 2 (I) | Interrupt Disable | 中断禁用标志 |
| 3 (D) | Decimal Mode | **NES 中无效**（6502 BCD 模式被移除） |
| 4 (B) | Break | BRK 指令触发时置位 |
| 5 | - | 未使用，始终为1 |
| 6 (V) | Overflow | 溢出标志（有符号溢出） |
| 7 (N) | Negative | 负数标志（结果最高位为1） |

## 完整指令集（按操作码排列）

### ADC - 加法（带进位）

| 寻址模式 | 操作码 | 字节数 | 周期数 |
|----------|--------|--------|--------|
| Immediate | $69 | 2 | 2 |
| Zero Page | $65 | 2 | 3 |
| Zero Page,X | $75 | 2 | 4 |
| Absolute | $6D | 3 | 4 |
| Absolute,X | $7D | 3 | 4+ |
| Absolute,Y | $79 | 3 | 4+ |
| Indirect,X | $61 | 2 | 6 |
| Indirect,Y | $71 | 2 | 5+ |

> `+` 表示跨页时额外增加1个周期

### AND - 逻辑与

| 寻址模式 | 操作码 | 字节数 | 周期数 |
|----------|--------|--------|--------|
| Immediate | $29 | 2 | 2 |
| Zero Page | $25 | 2 | 3 |
| Zero Page,X | $35 | 2 | 4 |
| Absolute | $2D | 3 | 4 |
| Absolute,X | $3D | 3 | 4+ |
| Absolute,Y | $39 | 3 | 4+ |
| Indirect,X | $21 | 2 | 6 |
| Indirect,Y | $31 | 2 | 5+ |

### ASL - 算术左移

| 寻址模式 | 操作码 | 字节数 | 周期数 |
|----------|--------|--------|--------|
| Accumulator | $0A | 1 | 2 |
| Zero Page | $06 | 2 | 5 |
| Zero Page,X | $16 | 2 | 6 |
| Absolute | $0E | 3 | 6 |
| Absolute,X | $1E | 3 | 7 |

### Branch Instructions - 分支指令

| 助记符 | 操作码 | 条件 | 周期数 |
|--------|--------|------|--------|
| BPL $xx | $10 | N=0 | 2 (+1 if branch taken, +1 if to new page) |
| BMI $xx | $30 | N=1 | 2 (+1 if branch taken, +1 if to new page) |
| BVC $xx | $50 | V=0 | 2 (+1 if branch taken, +1 if to new page) |
| BVS $xx | $70 | V=1 | 2 (+1 if branch taken, +1 if to new page) |
| BCC $xx | $90 | C=0 | 2 (+1 if branch taken, +1 if to new page) |
| BCS $xx | $B0 | C=1 | 2 (+1 if branch taken, +1 if to new page) |
| BNE $xx | $D0 | Z=0 | 2 (+1 if branch taken, +1 if to new page) |
| BEQ $xx | $F0 | Z=1 | 2 (+1 if branch taken, +1 if to new page) |

### BIT - 位测试

| 寻址模式 | 操作码 | 字节数 | 周期数 |
|----------|--------|--------|--------|
| Zero Page | $24 | 2 | 3 |
| Absolute | $2C | 3 | 4 |

### BRK - 强制中断

| 寻址模式 | 操作码 | 字节数 | 周期数 |
|----------|--------|--------|--------|
| Implied | $00 | 1 | 7 |

### Flag Instructions - 标志位操作

| 助记符 | 操作码 | 说明 | 周期数 |
|--------|--------|------|--------|
| CLC | $18 | C=0 | 2 |
| SEC | $38 | C=1 | 2 |
| CLI | $58 | I=0 | 2 |
| SEI | $78 | I=1 | 2 |
| CLV | $B8 | V=0 | 2 |
| CLD | $D8 | D=0 | 2 |
| SED | $F8 | D=1 | 2 |

> 注意：NES 的 6502 变体没有 BCD 模式，CLD/SED 实际上无效果

### CMP/CPX/CPY - 比较

| 指令 | 寻址模式 | 操作码 | 字节数 | 周期数 |
|------|----------|--------|--------|--------|
| CMP | Immediate | $C9 | 2 | 2 |
| CMP | Zero Page | $C5 | 2 | 3 |
| CMP | Zero Page,X | $D5 | 2 | 4 |
| CMP | Absolute | $CD | 3 | 4 |
| CMP | Absolute,X | $DD | 3 | 4+ |
| CMP | Absolute,Y | $D9 | 3 | 4+ |
| CMP | Indirect,X | $C1 | 2 | 6 |
| CMP | Indirect,Y | $D1 | 2 | 5+ |
| CPX | Immediate | $E0 | 2 | 2 |
| CPX | Zero Page | $E4 | 2 | 3 |
| CPX | Absolute | $EC | 3 | 4 |
| CPY | Immediate | $C0 | 2 | 2 |
| CPY | Zero Page | $C4 | 2 | 3 |
| CPY | Absolute | $CC | 3 | 4 |

### DEC/DEX/DEY - 递减

| 指令 | 寻址模式 | 操作码 | 字节数 | 周期数 |
|------|----------|--------|--------|--------|
| DEC | Zero Page | $C6 | 2 | 5 |
| DEC | Zero Page,X | $D6 | 2 | 6 |
| DEC | Absolute | $CE | 3 | 6 |
| DEC | Absolute,X | $DE | 3 | 7 |
| DEX | Implied | $CA | 1 | 2 |
| DEY | Implied | $88 | 1 | 2 |

### EOR - 逻辑异或

| 寻址模式 | 操作码 | 字节数 | 周期数 |
|----------|--------|--------|--------|
| Immediate | $49 | 2 | 2 |
| Zero Page | $45 | 2 | 3 |
| Zero Page,X | $55 | 2 | 4 |
| Absolute | $4D | 3 | 4 |
| Absolute,X | $5D | 3 | 4+ |
| Absolute,Y | $59 | 3 | 4+ |
| Indirect,X | $41 | 2 | 6 |
| Indirect,Y | $51 | 2 | 5+ |

### INC/INX/INY - 递增

| 指令 | 寻址模式 | 操作码 | 字节数 | 周期数 |
|------|----------|--------|--------|--------|
| INC | Zero Page | $E6 | 2 | 5 |
| INC | Zero Page,X | $F6 | 2 | 6 |
| INC | Absolute | $EE | 3 | 6 |
| INC | Absolute,X | $FE | 3 | 7 |
| INX | Implied | $E8 | 1 | 2 |
| INY | Implied | $C8 | 1 | 2 |

### JMP/JSR/RTS/R
| 指令 | 寻址模式 | 操作码 | 字节数 | 周期数 |
|------|----------|--------|--------|--------|
| JMP | Absolute | $4C | 3 | 3 |
| JMP | Indirect | $6C | 3 | 5 |
| JSR | Absolute | $20 | 3 | 6 |
| RTS | Implied | $60 | 1 | 6 |
| RTI | Implied | $40 | 1 | 6 |

### LDA/LDX/LDY - 加载

| 指令 | 寻址模式 | 操作码 | 字节数 | 周期数 |
|------|----------|--------|--------|--------|
| LDA | Immediate | $A9 | 2 | 2 |
| LDA | Zero Page | $A5 | 2 | 3 |
| LDA | Zero Page,X | $B5 | 2 | 4 |
| LDA | Absolute | $AD | 3 | 4 |
| LDA | Absolute,X | $BD | 3 | 4+ |
| LDA | Absolute,Y | $B9 | 3 | 4+ |
| LDA | Indirect,X | $A1 | 2 | 6 |
| LDA | Indirect,Y | $B1 | 2 | 5+ |
| LDX | Immediate | $A2 | 2 | 2 |
| LDX | Zero Page | $A6 | 2 | 3 |
| LDX | Zero Page,Y | $B6 | 2 | 4 |
| LDX | Absolute | $AE | 3 | 4 |
| LDX | Absolute,Y | $BE | 3 | 4+ |
| LDY | Immediate | $A0 | 2 | 2 |
| LDY | Zero Page | $A4 | 2 | 3 |
| LDY | Zero Page,X | $B4 | 2 | 4 |
| LDY | Absolute | $AC | 3 | 4 |
| LDY | Absolute,X | $BC | 3 | 4+ |

### LSR - 逻辑右移

| 寻址模式 | 操作码 | 字节数 | 周期数 |
|----------|--------|--------|--------|
| Accumulator | $4A | 1 | 2 |
| Zero Page | $46 | 2 | 5 |
| Zero Page,X | $56 | 2 | 6 |
| Absolute | $4E | 3 | 6 |
| Absolute,X | $5E | 3 | 7 |

### NOP - 空操作

| 寻址模式 | 操作码 | 字节数 | 周期数 |
|----------|--------|--------|--------|
| Implied | $EA | 1 | 2 |

> 注意：NES 的 6502 变体还有一些未公开的操作码（如 $1A, $3A, $5A, $7A, $DA, $FA）也表现为 NOP，但官方未记录。

### ORA - 逻辑或

| 寻址模式 | 操作码 | 字节数 | 周期数 |
|----------|--------|--------|--------|
| Immediate | $09 | 2 | 2 |
| Zero Page | $05 | 2 | 3 |
| Zero Page,X | $15 | 2 | 4 |
| Absolute | $0D | 3 | 4 |
| Absolute,X | $1D | 3 | 4+ |
| Absolute,Y | $19 | 3 | 4+ |
| Indirect,X | $01 | 2 | 6 |
| Indirect,Y | $11 | 2 | 5+ |

### Push/Pop - 栈操作

| 指令 | 操作码 | 字节数 | 周期数 |
|------|--------|--------|--------|
| PHA | $48 | 1 | 3 |
| PHP | $08 | 1 | 3 |
| PLA | $68 | 1 | 4 |
| PLP | $28 | 1 | 4 |

### ROL/ROR - 循环移位

| 指令 | 寻址模式 | 操作码 | 字节数 | 周期数 |
|------|----------|--------|--------|--------|
| ROL | Accumulator | $2A | 1 | 2 |
| ROL | Zero Page | $26 | 2 | 5 |
| ROL | Zero Page,X | $36 | 2 | 6 |
| ROL | Absolute | $2E | 3 | 6 |
| ROL | Absolute,X | $3E | 3 | 7 |
| ROR | Accumulator | $6A | 1 | 2 |
| ROR | Zero Page | $66 | 2 | 5 |
| ROR | Zero Page,X | $76 | 2 | 6 |
| ROR | Absolute | $6E | 3 | 6 |
| ROR | Absolute,X | $7E | 3 | 7 |

### SBC - 减法（带借位）

| 寻址模式 | 操作码 | 字节数 | 周期数 |
|----------|--------|--------|--------|
| Immediate | $E9 | 2 | 2 |
| Zero Page | $E5 | 2 | 3 |
| Zero Page,X | $F5 | 2 | 4 |
| Absolute | $ED | 3 | 4 |
| Absolute,X | $FD | 3 | 4+ |
| Absolute,Y | $F9 | 3 | 4+ |
| Indirect,X | $E1 | 2 | 6 |
| Indirect,Y | $F1 | 2 | 5+ |

### STA/STX/STY - 存储

| 指令 | 寻址模式 | 操作码 | 字节数 | 周期数 |
|------|----------|--------|--------|--------|
| STA | Zero Page | $85 | 2 | 3 |
| STA | Zero Page,X | $95 | 2 | 4 |
| STA | Absolute | $8D | 3 | 4 |
| STA | Absolute,X | $9D | 3 | 5 |
| STA | Absolute,Y | $99 | 3 | 5 |
| STA | Indirect,X | $81 | 2 | 6 |
| STA | Indirect,Y | $91 | 2 | 6 |
| STX | Zero Page | $86 | 2 | 3 |
| STX | Zero Page,Y | $96 | 2 | 4 |
| STX | Absolute | $8E | 3 | 4 |
| STY | Zero Page | $84 | 2 | 3 |
| STY | Zero Page,X | $94 | 2 | 4 |
| STY | Absolute | $8C | 3 | 4 |

### Transfer Instructions - 寄存器传送

| 指令 | 操作码 | 说明 | 周期数 |
|------|--------|------|--------|
| TAX | $AA | A → X | 2 |
| TXA | $8A | X → A | 2 |
| TAY | $A8 | A → Y | 2 |
| TYA | $98 | Y → A | 2 |
| TSX | $BA | SP → X | 2 |
| TXS | $9A | X → SP | 2 |

## 未记录操作码（Undocumented Opcodes）

NES 的 6502 变体（RP2A03/2A07）存在一些未官方记录但可用的操作码：

| 操作码 | 助记符 | 说明 |
|--------|--------|------|
| $0B | ANC #imm | A AND #imm，设置 C = bit 7 |
| $2B | ANC #imm | 同上 |
| $4B | ALR #imm | A AND #imm，然后 LSR A |
| $6B | ARR #imm | A AND #imm，然后 ROR A，特殊 V 标志处理 |
| $8B | XAA #imm | 未稳定，部分机器上表现不同 |
| $AB | ATX #imm | A = X = (A OR $EE) AND #imm |
| $CB | AXS #imm | X = A AND X，然后 X = X - #imm |
| $EB | SBC #imm | 同 $E9（额外 SBC） |

> 注意：部分未记录操作码在不同批次的芯片上表现不一致，模拟器可选择不实现它们。

## 中断

| 中断类型 | 向量地址 | 说明 |
|----------|----------|------|
| NMI | $FFFA/$FFFB | 非屏蔽中断（PPU 每帧触发） |
| RESET | $FFFC/$FFFD | 复位向量 |
| IRQ/BRK | $FFFE/$FFFF | 中断请求向量 |

## 参考来源

- [MOS 6502 Programming Manual](https://www.mos.com/6500/)
- [NESDEV Wiki - 6502 reference](https://www.nesdev.org/wiki/CPU)
- [6502.org](https://6502.org/)
