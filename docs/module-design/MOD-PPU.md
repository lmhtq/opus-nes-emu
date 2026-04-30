# MOD-PPU: PPU 模拟器

## 元数据 (Metadata)

- **ID**: MOD-PPU
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-002
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 功能职责 (Responsibilities)

模拟 FC/NES 的 PPU（Picture Processing Unit）。

核心职责：
1. 实现 PPU 寄存器（$2000-$2007）的读写模拟
2. 渲染背景层（从名称表和图案表）
3. 渲染精灵层（从 OAM 和图案表）
4. 管理调色板（背景 $3F00-$3F0F，精灵 $3F10-$3F1F）
5. 处理扫描线级渲染时序（262 条扫描线）
6. 实现精灵 0 碰撞检测（Sprite 0 Hit）
7. 实现精灵溢出标志
8. 支持滚动（PPUSCROLL）
9. 触发 VBlank NMI（当 $2000 bit 7=1）
10. 支持名称表镜像（水平/垂直/四屏幕）

## 接口设计 (Interface Design)

```cpp
// include/fcemu/ppu.h
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace fcemu {

// PPU 寄存器
struct PpuRegisters {
    uint8_t ctrl;      // $2000 PPUCTRL
    uint8_t mask;      // $2001 PPUMASK
    uint8_t status;    // $2002 PPUSTATUS
    uint8_t oam_addr;  // $2003 OAMADDR
    uint8_t oam_data;  // $2004 OAMDATA
    uint8_t scroll;    // $2005 PPUSCROLL (latch)
    uint8_t addr;      // $2006 PPUADDR (latch)
    uint8_t data;      // $2007 PPUDATA
};

// 帧缓冲区（RGBA 格式）
struct FrameBuffer {
    static constexpr int WIDTH = 256;
    static constexpr int HEIGHT = 240;
    uint8_t pixels[WIDTH * HEIGHT * 4];  // RGBA
};

// 内存读写回调（PPU 地址空间）
using PpuReadCallback = std::function<uint8_t(uint16_t addr)>;
using PpuWriteCallback = std::function<void(uint16_t addr, uint8_t val)>;

// NMI 回调
using NmiCallback = std::function<void()>;

class Ppu {
public:
    Ppu();

    void reset();
    void set_callbacks(PpuReadCallback read, PpuWriteCallback write,
                      NmiCallback nmi);

    // CPU 侧寄存器访问
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t val);

    // PPU 执行一个周期（或一条扫描线）
    void step(int cycles);
    void step_scanline();

    // 获取当前帧（用于显示）
    const FrameBuffer& frame_buffer() const { return frame_buffer_; }

    // 触发 NMI（由内部逻辑调用）
    void signal_nmi();

    // 精灵 0 碰撞检测
    bool check_sprite0_hit();

    // 读取 PPU 状态（用于 CPU）
    uint8_t read_status();

    // 写入 PPU 地址/数据
    void write_addr(uint16_t addr);
    void write_data(uint8_t val);
    uint8_t read_data();

private:
    PpuRegisters regs_;
    FrameBuffer frame_buffer_;

    // 内部状态
    uint16_t vram_addr_;     // 当前 VRAM 地址（15-bit）
    uint16_t temp_addr_;     // 临时地址（用于 $2006 写入）
    uint8_t fine_x_;        // 精细 X 滚动
    bool addr_latch_;        // $2005/$2006 写入锁存器

    // OAM（64 个精灵 × 4 字节）
    uint8_t oam_[256];
    uint8_t secondary_oam_[32];  // 当前扫描线的精灵（8 个 × 4 字节）

    // 移位寄存器（背景渲染）
    uint16_t bg_shift_lo_;
    uint16_t bg_shift_hi_;
    uint8_t bg_attr_lo_;
    uint8_t bg_attr_hi_;

    // 扫描线状态
    int scanline_;
    int dot_;
    bool even_frame_;

    // 回调
    PpuReadCallback read_;
    PpuWriteCallback write_;
    NmiCallback nmi_callback_;

    // 渲染函数
    void render_scanline(int y);
    void evaluate_sprites(int y);
    void fetch_bg_tile(int x);
    void fetch_sprite_tile(int sprite_idx);

    // 调色板
    uint8_t palette_[32];  // $3F00-$3F1F
};

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-MEMORY | PPU 通过名称表/图案表访问 VRAM |
| MOD-CARTRIDGE | CHR ROM/RAM 和镜像设置 |
| MOD-CPU | PPU 触发 NMI 到 CPU |

## 数据结构 (Data Structures)

### OAM 精灵条目

```cpp
struct OamEntry {
    uint8_t y;          // Y 位置（屏幕 Y = y + 1）
    uint8_t tile;       // Tile 索引
    uint8_t attr;       // 属性（调色板/翻转/优先级）
    uint8_t x;          // X 位置
};

// 属性字节位域
// bit 0-1: palette (0-3)
// bit 5: priority (0=foreground, 1=background)
// bit 6: flip horizontal
// bit 7: flip vertical
```

### 扫描线状态

```cpp
struct ScanlineState {
    int number;           // 当前扫描线 0-261 (NTSC)
    int dot;              // 当前像素点 0-340
    bool is_visible;       // 可见扫描线 0-239
    bool is_vblank;        // VBlank 期间 241-261
    bool is_prerender;     // 预渲染扫描线 261
};
```

## 状态机 (State Machines)

### PPU 扫描线状态

```
[Scanline 0-239: Visible]
    → [Scanline 240: Idle]
        → [Scanline 241: VBlank Start, set $2002 bit 7]
            → [Scanlines 242-260: VBlank, NMI if enabled]
                → [Scanline 261: Pre-render, clear $2002 bit 7]
                    → back to Scanline 0
```

### 精灵评估（Sprite Evaluation）

```
[Scanline 257-320]
    → [Clear secondary OAM]
        → [For each of 64 sprites]:
            → [Check Y range: Y <= scanline < Y+8(or 16)?]
                → Yes: [Add to secondary OAM (max 8)]
                → No: [Skip]
                    → [If >8 sprites: set overflow flag]
```

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/ppu/registers.md` - PPU 寄存器参考
- `docs/hardware/ppu/rendering.md` - 渲染流程参考
- `docs/hardware/ppu/sprites.md` - 精灵处理参考

## 变更记录 (Change History)

- 2026-04-30: Initial version
