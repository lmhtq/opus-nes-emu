# PPU 寄存器参考

PPU（Picture Processing Unit）是 FC/NES 的图形处理单元，拥有 8 个寄存器，映射到 CPU 地址空间的 $2000-$2007。

## PPU 寄存器映射

| 地址 | 名称 | 说明 |
|------|------|------|
| $2000 | PPUCTRL | 控制器寄存器（write） |
| $2001 | PPUMASK | 掩码寄存器（write） |
| $2002 | PPUSTATUS | 状态寄存器（read） |
| $2003 | OAMADDR | OAM 地址（write） |
| $2004 | OAMDATA | OAM 数据（read/write） |
| $2005 | PPUSCROLL | 滚动寄存器（write x2） |
| $2006 | PPUADDR | PPU 地址（write x2） |
| $2007 | PPUDATA | PPU 数据（read/write） |

> 注意：$2000-$2007 是镜像地址，实际只使用了 3 位地址线（A2-A0），$2008-$3FFF 都是 $2000-$2007 的镜像。

## $2000 - PPUCTRL（控制器）

| 位 | 名称 | 说明 |
|----|------|------|
| 0 | NMI Enable (V) | VBlank 期间是否触发 NMI（1=允许） |
| 1 | PPU Master/Slave | **NES 上未使用**（始终为 0） |
| 2 | Sprite Size | 精灵大小（0=8x8, 1=8x16） |
| 3 | Sprite Pattern Table | 8x16 模式下的图案表基地址（0=$0000, 1=$1000） |
| 4 | Background Pattern Table | 背景图案表基地址（0=$0000, 1=$1000） |
| 5 | VRAM Increment | VRAM 地址增量（0=+1, 1=+32） |
| 6-7 | Base Nametable Address | 名称表基地址 |
| | | 00=$2000, 01=$2400, 10=$2800, 11=$2C00 |

## $2001 - PPUMASK（掩码）

| 位 | 名称 | 说明 |
|----|------|------|
| 0 | Grayscale | 1=仅显示灰度 |
| 1 | Show Background in leftmost 8 pixels | 0=隐藏最左 8 像素的背景 |
| 2 | Show Sprites in leftmost 8 pixels | 0=隐藏最左 8 像素的精灵 |
| 3 | Show Background | 1=显示背景 |
| 4 | Show Sprites | 1=显示精灵 |
| 5 | Intensify Reds | 增强红色（NTSC 色调控制） |
| 6 | Intensify Greens | 增强绿色 |
| 7 | Intensify Blues | 增强蓝色 |

## $2002 - PPUSTATUS（状态）

| 位 | 名称 | 说明 |
|----|------|------|
| 0-4 | - | 未使用（返回 0） |
| 5 | Sprite Overflow | 精灵溢出标志（超过 8 个精灵在同一扫描线） |
| 6 | Sprite 0 Hit | 精灵 0 碰撞标志 |
| 7 | Vertical Blank (VBlank) | 1=正在 VBlank 期间 |

> 读取 $2002 会清除 $2005/$2006 的 latch（锁存器）。

## $2003 - OAMADDR（OAM 地址）

设置 OAM（Object Attribute Memory，精灵属性内存）的访问地址（0-$FF）。

## $2004 - OAMDATA（OAM 数据）

读取或写入 OAM。每次访问后 OAMADDR 自动递增。

OAM 每个精灵占 4 字节：
| 字节 | 说明 |
|------|------|
| 0 | Y 位置（屏幕 Y - 1） |
| 1 | 图案索引（Tile 编号） |
| 2 | 属性（调色板、翻转等） |
| 3 | X 位置 |

## $2005 - PPUSCROLL（滚动）

写入两次：
1. 第一次：X 滚动（水平）
2. 第二次：Y 滚动（垂直）

## $2006 - PPUADDR（PPU 地址）

写入两次：
1. 第一次：高字节
2. 第二次：低字节

## $2007 - PPUDATA（PPU 数据）

读取或写入 PPU VRAM（图案表、名称表等）。每次访问后地址自动增加（由 $2000 的 bit 2 控制：+1 或 +32）。

> 读取 $2007 时，第一个读操作返回的是"缓冲"值，实际 VRAM 值在下一次读取时返回。

## PPU 时序

- 每秒 60.0988 帧（NTSC）/ 50.0070 帧（PAL）
- 每帧 262 扫描线（NTSC）/ 312 扫描线（PAL）
- 可见扫描线：0-239（240 条扫描线）
- VBlank：扫描线 241-261
- 预渲染扫描线：261（NTSC）/ 311（PAL）

## 参考来源

- [NESDEV Wiki - PPU Registers](https://www.nesdev.org/wiki/PPU_registers)
- [NESDEV Wiki - PPU](https://www.nesdev.org/wiki/PPU)
