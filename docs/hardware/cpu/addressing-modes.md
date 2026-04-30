# 6502 CPU 寻址模式参考

FC/NES 使用的 6502 处理器支持 13 种寻址模式。

## 寻址模式列表

### 1. Implied（隐含寻址）

操作数隐含在指令中，无需额外操作数。

```
TPA  ; 隐含寻址，操作码自身包含操作数信息
```

指令示例：TAX, TXA, TAY, TYA, TSX, TXS, PHA, PHP, PLA, PLP, INX, INY, DEX, DEY, NOP, RTI, RTS, CLC, SEC, CLI, SEI, CLV, CLD, SED, BRK

### 2. Accumulator（累加器寻址）

操作对象是累加器 A 本身。

```
ASL A  ; 累加器左移（操作码 $0A）
LSR A  ; 累加器右移（操作码 $4A）
ROL A  ; 累加器循环左移（操作码 $2A）
ROR A  ; 累加器循环右移（操作码 $6A）
```

### 3. Immediate（立即寻址）

操作数直接包含在指令中，以 `#` 前缀表示。

```
LDA #$10  ; 加载立即数 $10 到 A（操作码 $A9）
ADC #$FF  ; A = A + $FF + C
```

- 操作码后缀：`#` 值
- 字节数：2（操作码 + 1字节立即数）
- 寻址模式字节：`#imm`

### 4. Zero Page（零页寻址）

操作数位于零页（$00-$FF），只需一个字节地址。

```
LDA $20  ; 从地址 $20 加载值到 A（操作码 $A5）
```

- 地址范围：$0000 - $00FF
- 字节数：2（操作码 + 1字节地址）
- 比绝对寻址少1字节，快1个周期

### 5. Zero Page,X（零页X变址）

在零页地址上加上 X 寄存器的值得到有效地址。

```
LDX #$05
LDA $20,X  ; 实际地址 = $20 + X = $25（操作码 $B5）
```

- 地址计算：如果结果超过 $FF，会回绕（wrap around）
- 字节数：2（操作码 + 1字节地址）

### 6. Zero Page,Y（零页Y变址）

类似 Zero Page,X，但使用 Y 寄存器。仅用于 LDX 和 STX。

```
LDY #$03
LDX $10,Y  ; 实际地址 = $10 + Y = $13（操作码 $B6）
```

### 7. Absolute（绝对寻址）

使用完整的 16 位地址。

```
LDA $1234  ; 从地址 $1234 加载值到 A（操作码 $AD）
```

- 地址范围：$0000 - $FFFF
- 字节数：3（操作码 + 低字节 + 高字节）
- 注意：小端序存储，低字节在前

### 8. Absolute,X（绝对X变址）

在绝对地址上加上 X 寄存器的值。

```
LDX #$10
LDA $1000,X  ; 实际地址 = $1000 + X = $1010（操作码 $BD）
```

- 跨页检查：如果跨越了 256 字节页面边界，额外增加1个周期
- 字节数：3

### 9. Absolute,Y（绝对Y变址）

类似 Absolute,X，使用 Y 寄存器。

```
LDY #$20
LDA $2000,Y  ; 实际地址 = $2000 + Y = $2020
```

### 10. Indirect,X（间接X变址/先变址后间接）

用 Zero Page 地址加上 X，然后从该地址读取一个16位指针，再访问该指针指向的地址。

```
LDX #$04
LDA ($10,X)  ; 1. 计算 $10 + X = $14
              ; 2. 从 $0014 读低字节，从 $0015 读高字节，得到指针 addr
              ; 3. 从 addr 加载值到 A
```

- 也称为 "( indirect,X )" 或 "pre-indexed indirect"
- 常用于访问函数指针表

### 11. Indirect,Y（间接Y变址/后变址间接）

先从 Zero Page 读取一个16位指针，然后加上 Y 寄存器。

```
LDY #$20
LDA ($10),Y  ; 1. 从 $0010 读低字节，从 $0011 读高字节，得到指针 addr
              ; 2. 计算 addr + Y = 实际地址
              ; 3. 从实际地址加载值到 A
```

- 也称为 "( indirect ),Y" 或 "post-indexed indirect"
- 常用于访问数据结构中的字段

### 12. Relative（相对寻址）

用于分支指令（BEQ, BNE, BCC 等），操作数是一个有符号的8位偏移量。

```
BEQ $1234  ; 如果 Z=1，则 PC = PC + offset
            ; offset 是有符号数，范围 -128 ~ +127
```

- 分支范围：当前指令结束位置 ±127 字节
- 周期：分支未发生时 2 周期；发生时 3 周期；跨页时 4 周期

### 13. Indirect（间接寻址）

仅用于 JMP 指令，从一个16位指针地址读取跳转目标地址。

```
JMP ($1234)  ; 从 $1234 读低字节，从 $1235 读高字节，跳转到该地址
```

- **注意**：存在硬件 Bug —— 如果指针低字节为 $FF（如 $12FF），高字节读取不会进位到下一页，而是从 $12FF 和 $1200 读取。
- 这是 6502 的已知缺陷，模拟器需要正确模拟此行为

## 寻址模式汇总表

| 模式 | 汇编语法 | 示例 | 字节数 | 周期数 |
|------|----------|------|--------|--------|
| Implied | `OP` | `CLC` | 1 | 2 |
| Accumulator | `OP A` | `ASL A` | 1 | 2 |
| Immediate | `OP #$nn` | `LDA #$10` | 2 | 2 |
| Zero Page | `OP $nn` | `LDA $20` | 2 | 3 |
| Zero Page,X | `OP $nn,X` | `LDA $20,X` | 2 | 4 |
| Zero Page,Y | `OP $nn,Y` | `LDX $10,Y` | 2 | 4 |
| Absolute | `OP $nnnn` | `LDA $1234` | 3 | 4 |
| Absolute,X | `OP $nnnn,X` | `LDA $1234,X` | 3 | 4+ |
| Absolute,Y | `OP $nnnn,Y` | `LDA $1234,Y` | 3 | 4+ |
| Indirect,X | `OP ($nn,X)` | `LDA ($10,X)` | 2 | 6 |
| Indirect,Y | `OP ($nn),Y` | `LDA ($10),Y` | 2 | 5+ |
| Relative | `OP $nnnn` | `BEQ $1234` | 2 | 2/3/4 |
| Indirect | `JMP ($nnnn)` | `JMP ($1234)` | 3 | 5 |

> `+` 表示跨页时额外增加1个周期

## 参考来源

- [NESDEV Wiki - Addressing Modes](https://www.nesdev.org/wiki/Addressing_modes)
- [6502.org - Addressing Modes](https://6502.org/tutorials/addressing_modes.html)
