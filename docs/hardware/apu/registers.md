# APU 寄存器参考

FC/NES 的 APU（Audio Processing Unit）包含多个音频通道，寄存器映射到 CPU 地址空间 $4000-$4017。

## APU 寄存器映射

| 地址 | 名称 | 说明 |
|------|------|------|
| $4000-$4003 | Pulse 1 (方形波 1) | 第一个脉冲波通道 |
| $4004-$4007 | Pulse 2 (方形波 2) | 第二个脉冲波通道 |
| $4008-$400B | Triangle (三角波) | 三角波通道 |
| $400C-$400F | - | **未使用**（空置） |
| $4010-$4013 | Noise (噪声) | 噪声通道 |
| $4014 | DMA OAM | 直接内存访问，用于快速写入 OAM |
| $4015 | APU Status / SNDCHN | 音频通道使能与状态 |
| $4016 | JOY1 / APU Control | 手柄 1 数据 / DMC 控制 |
| $4017 | JOY2 / APU Frame Counter | 手柄 2 数据 / 帧计数器 |

## 脉冲波通道（Pulse 1 & Pulse 2）

每个脉冲波通道有 4 个寄存器：

### $4000 / $4004 - 控制寄存器（Pulse Control）

| 位 | 名称 | 说明 |
|----|------|------|
| 0-3 | Volume / Envelope | 音量或包络衰减速度 |
| 4 | Envelope Loop / Length Halt | 包络循环 / 长度计数器停止 |
| 5 | Constant Volume | 1=固定音量，0=包络衰减 |
| 6-7 | Duty Cycle | 占空比（00=12.5%, 01=25%, 10=50%, 11=75%） |

### $4001 / $4005 - 扫频（Sweep）

| 位 | 名称 | 说明 |
|----|------|------|
| 0-2 | Sweep Shift | 扫频移位次数 |
| 3 | Sweep Negate | 1=负向扫频（频率降低） |
| 4-6 | Sweep Period | 扫频周期（每 N 个帧更新一次） |
| 7 | Sweep Enable | 扫频使能 |

### $4002 / $4006 - 定时器低字节（Timer Low）

设置频率定时器的低 8 位。

### $4003 / $4007 - 定时器高字节 + 长度（Timer High + Length）

| 位 | 名称 | 说明 |
|----|------|------|
| 0-2 | Timer High | 定时器高 3 位（共 11 位定时器） |
| 3-7 | Length Counter Load | 长度计数器载入值 |

## 三角波通道（Triangle）

### $4008 - 线性计数器控制（Linear Counter Control）

| 位 | 名称 | 说明 |
|----|------|------|
| 0-6 | Linear Counter Load | 线性计数器载入值 |
| 7 | Linear Counter Control | 1=控制位（与长度计数器 halt 类似） |

### $4009 - 未使用

### $400A - 定时器低字节（Timer Low）

### $400B - 定时器高字节 + 长度（Timer High + Length）

| 位 | 名称 | 说明 |
|----|------|------|
| 0-2 | Timer High | 定时器高 3 位 |
| 3-7 | Length Counter Load | 长度计数器载入值 |

## 噪声通道（Noise）

### $400C - 控制（Noise Control）

| 位 | 名称 | 说明 |
|----|------|------|
| 0-3 | Volume / Envelope | 音量或包络 |
| 4 | Envelope Loop / Length Halt | 同脉冲波 |
| 5 | Constant Volume | 同脉冲波 |
| 6-7 | 未使用 | - |

### $400D - 未使用

### $400E - 噪声模式（Noise Mode）

| 位 | 名称 | 说明 |
|----|------|------|
| 0-3 | Noise Period | 噪声周期（查表） |
| 4-6 | 未使用 | - |
| 7 | Loop Mode | 0=短周期（15位噪声）, 1=长周期（1位噪声） |

### $400F - 长度计数器（Length Counter Load）

## $4015 - APU 状态（SNDCHN）

### 写入 $4015（APU 控制）

| 位 | 名称 | 说明 |
|----|------|------|
| 0 | Enable Pulse 1 | 1=使能脉冲波 1 |
| 1 | Enable Pulse 2 | 1=使能脉冲波 2 |
| 2 | Enable Triangle | 1=使能三角波 |
| 3 | Enable Noise | 1=使能噪声 |
| 4 | Enable DMC | 1=使能 DMC（Delta Modulation Channel） |
| 5-7 | 未使用 | - |

### 读取 $4015（APU 状态）

| 位 | 名称 | 说明 |
|----|------|------|
| 0 | Pulse 1 Length | 脉冲波 1 长度计数器 > 0 |
| 1 | Pulse 2 Length | 脉冲波 2 长度计数器 > 0 |
| 2 | Triangle Length | 三角波长度计数器 > 0 |
| 3 | Noise Length | 噪声长度计数器 > 0 |
| 4 | DMC Interrupt | DMC 中断标志 |
| 5 | Frame Interrupt | 帧中断标志 |
| 6-7 | 未使用 | - |

## $4017 - 帧计数器（Frame Counter）

| 位 | 名称 | 说明 |
|----|------|------|
| 0-6 | 未使用 | - |
| 7 | Mode | 0=4步模式，1=5步模式 |
| | | 4步：每 29830.5 CPU 周期触发（约 60.0988Hz × 4） |
| | | 5步：每 3728.5 CPU 周期触发（约 60.0988Hz × 5） |

## 音频频率计算

### 脉冲波和三角波频率

```
定时器值 = (CPU时钟 / (16 × 目标频率)) - 1   （NTSC CPU 时钟 = 1.789772 MHz）
```

| 音符 | 频率 (Hz) | 定时器值（近似） |
|------|-----------|------------------|
| C4 | 261.63 | $06B5 |
| D4 | 293.66 | $05F8 |
| E4 | 329.63 | $054C |
| F4 | 349.23 | $04F3 |
| G4 | 392.00 | $0457 |
| A4 | 440.00 | $03E0 |
| B4 | 493.88 | $0374 |
| C5 | 523.25 | $035B |

## 参考来源

- [NESDEV Wiki - APU](https://www.nesdev.org/wiki/APU)
- [NESDEV Wiki - APU Pulse](https://www.nesdev.org/wiki/APU_pulse)
- [NESDEV Wiki - APU Triangle](https://www.nesdev.org/wiki/APU_triangle)
- [NESDEV Wiki - APU Noise](https://www.nesdev.org/wiki/APU_noise)
