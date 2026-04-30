# MOD-MEMORY: 内存映射

## 元数据 (Metadata)

- **ID**: MOD-MEMORY
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-004
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 功能职责 (Responsibilities)

实现 FC/NES CPU 地址空间的内存映射（$0000-$FFFF）。

核心职责：
1. 实现 2KB 内部 RAM（$0000-$07FF）及其镜像
2. 映射 PPU 寄存器（$2000-$2007 及镜像）
3. 映射 APU/IO 寄存器（$4000-$4017）
4. 映射卡带空间（$4020-$FFFF）
5. 支持 PRG RAM（电池备份 SRAM $6000-$7FFF）
6. 提供统一的内存读写接口
7. 支持 Mapper 注册内存回调

## 接口设计 (Interface Design)

```cpp
// include/fcemu/memory.h
#pragma once

#include <cstdint>
#include <functional>
#include <vector>
#include <array>

namespace fcemu {

// 内存读写回调（用于 Mapper 注册）
using MemReadCallback = std::function<uint8_t(uint16_t addr)>;
using MemWriteCallback = std::function<void(uint16_t addr, uint8_t val)>;

class Memory {
public:
    Memory();

    void reset();

    // 基本读写
    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t val);

    // 直接访问内部 RAM（用于 DMA）
    uint8_t* internal_ram() { return ram_.data(); }
    const uint8_t* internal_ram() const { return ram_.data(); }

    // 注册 Mapper 回调
    void register_mapper_read(uint16_t start, uint16_t end, MemReadCallback cb);
    void register_mapper_write(uint16_t start, uint16_t end, MemWriteCallback cb);

    // 注册 PPU/APU 回调
    void set_ppu_callbacks(MemReadCallback read, MemWriteCallback write);
    void set_apu_callbacks(MemReadCallback read, MemWriteCallback write);

    // PRG RAM（电池备份）
    bool has_battery_ram() const { return has_battery_ram_; }
    void set_battery_ram_size(size_t size);
    uint8_t* battery_ram() { return battery_ram_.data(); }

    // 加载 PRG ROM
    void load_prg_rom(const std::vector<uint8_t>& data);
    void set_prg_rom_bank(int slot, size_t bank);  // 用于 Mapper

private:
    // 内部 RAM 2KB + 镜像
    std::array<uint8_t, 0x0800> ram_;

    // PRG ROM（最大 512KB）
    std::vector<uint8_t> prg_rom_;

    // PRG RAM（电池备份，最大 8KB）
    std::vector<uint8_t> battery_ram_;
    bool has_battery_ram_;

    // Mapper 回调
    MemReadCallback mapper_read_;
    MemWriteCallback mapper_write_;
    uint16_t mapper_range_start_;
    uint16_t mapper_range_end_;

    // PPU/APU 回调
    MemReadCallback ppu_read_;
    MemWriteCallback ppu_write_;
    MemReadCallback apu_read_;
    MemWriteCallback apu_write_;

    // 辅助函数
    uint16_t mirror_ram_addr(uint16_t addr) const;
    uint16_t mirror_ppu_addr(uint16_t addr) const;
};

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-CARTRIDGE | Mapper 注册内存回调，控制 $4020-$FFFF |
| MOD-PPU | PPU 寄存器通过内存映射访问 |
| MOD-APU | APU 寄存器通过内存映射访问 |

## 数据结构 (Data Structures)

### 内存区域

```cpp
struct MemoryRegion {
    uint16_t start;
    uint16_t end;
    MemReadCallback read_cb;
    MemWriteCallback write_cb;
};
```

### 完整内存映射

```
$0000-$07FF: Internal RAM (2KB)
$0800-$1FFF: Mirror of $0000-$07FF
$2000-$2007: PPU Registers
$2008-$3FFF: Mirror of $2000-$2007
$4000-$4017: APU/IO Registers
$4018-$401F: Unused
$4020-$5FFF: Expansion (Mapper)
$6000-$7FFF: PRG RAM (Battery, optional, 8KB)
$8000-$BFFF: PRG ROM (16KB bank)
$C000-$FFFF: PRG ROM (16KB bank or mirror)
```

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/memory/memory-map.md` - NES 内存映射参考
- `docs/hardware/cartridge/mappers.md` - Mapper 参考
- `docs/hardware/cartridge/rom-format.md` - iNES 格式参考

## 变更记录 (Change History)

- 2026-04-30: Initial version
