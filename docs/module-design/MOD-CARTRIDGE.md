# MOD-CARTRIDGE: 卡带与 Mapper

## 元数据 (Metadata)

- **ID**: MOD-CARTRIDGE
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-004
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 功能职责 (Responsibilities)

实现 FC/NES 卡带加载和 Mapper（映射器）系统。

核心职责：
1. 解析 iNES ROM 格式（.nes 文件）
2. 加载 PRG ROM、CHR ROM/RAM
3. 管理电池备份 RAM（PRG RAM $6000-$7FFF）
4. 实现 Mapper 0（NROM）- 无银行切换
5. 实现 Mapper 1（MMC1）- 串口银行切换
6. 实现 Mapper 2（UxROM）- PRG ROM 切换
7. 实现 Mapper 3（CNROM）- CHR ROM 切换
8. 实现 Mapper 4（MMC3）- 复杂切换 + IRQ
9. 提供统一的内存读写接口给 CPU/PPU

## 接口设计 (Interface Design)

```cpp
// include/fcemu/cartridge.h
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace fcemu {

// iNES ROM 头部
struct INesHeader {
    char magic[4];      // "NES\x1a"
    uint8_t prg_rom_size; // PRG ROM banks (16KB each)
    uint8_t chr_rom_size; // CHR ROM banks (8KB each), 0 = CHR RAM
    uint8_t flags6;
    uint8_t flags7;
    uint8_t prg_ram_size; // PRG RAM (8KB each), 0 = 8KB
    uint8_t flags9;
    uint8_t flags10;
    uint8_t reserved[5];
};

// Mapper 接口
class Mapper {
public:
    virtual ~Mapper() = default;
    virtual uint8_t cpu_read(uint16_t addr) = 0;
    virtual void cpu_write(uint16_t addr, uint8_t val) = 0;
    virtual uint8_t ppu_read(uint16_t addr) = 0;
    virtual void ppu_write(uint16_t addr, uint8_t val) = 0;
    virtual void scanline_irq(int scanline) { }  // Mapper 4 等使用
    virtual void reset() = 0;
};

// 卡带类
class Cartridge {
public:
    Cartridge();
    ~Cartridge();

    // 加载 ROM 文件
    bool load_rom(const std::string& path);
    bool load_rom_data(const std::vector<uint8_t>& data);

    // 获取信息
    int mapper_number() const { return mapper_number_; }
    const std::string& game_name() const { return game_name_; }
    bool has_battery() const { return has_battery_; }

    // CPU 侧访问
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t val);

    // PPU 侧访问
    uint8_t ppu_read(uint16_t addr);
    void ppu_write(uint16_t addr, uint8_t val);

    // 电池备份 RAM
    uint8_t* battery_ram() { return battery_ram_.data(); }
    void save_battery_ram(const std::string& path);
    void load_battery_ram(const std::string& path);

    // Mapper IRQ
    bool irq_pending() const { return irq_pending_; }
    void clear_irq() { irq_pending_ = false; }

    // 扫描线通知（用于 Mapper 4 IRQ）
    void notify_scanline(int scanline);

    // 镜像类型
    enum class MirrorMode { Horizontal, Vertical, FourScreen, Single0, Single1 };
    MirrorMode mirror_mode() const { return mirror_mode_; }

private:
    std::vector<uint8_t> prg_rom_;
    std::vector<uint8_t> chr_rom_;
    std::vector<uint8_t> chr_ram_;  // 当 chr_rom_size_ == 0
    std::vector<uint8_t> battery_ram_;
    int mapper_number_;
    std::string game_name_;
    bool has_battery_;
    MirrorMode mirror_mode_;
    bool irq_pending_;

    std::unique_ptr<Mapper> mapper_;

    // Mapper 工厂
    static std::unique_ptr<Mapper> create_mapper(int number, Cartridge& cart);
};

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-MEMORY | 卡带注册回调到内存映射 |
| MOD-CPU | 卡带通过 $4020-$FFFF 被 CPU 访问 |
| MOD-PPU | CHR 通过 $0000-$1FFF 被 PPU 访问 |

## 数据结构 (Data Structures)

### iNES 头部标志位

```cpp
// flags6 bit 0: mirroring (0=horizontal/vertical, 1=vertical/horizontal)
// flags6 bit 1: battery
// flags6 bit 2: trainer (512 bytes before PRG ROM)
// flags6 bit 3: four-screen mirroring
// flags6 bit 4-7: mapper low bits
// flags7 bit 0-3: mapper high bits
// flags7 bit 4: VS Unisystem
// flags7 bit 5: PlayChoice-10
```

### Mapper 4（MMC3）内部状态

```cpp
struct MMC3State {
    uint8_t bank_select;     // $8000
    uint8_t bank_data[8];    // $8001, $A000, $C000, $E000 等
    uint8_t mirror;           // $A000
    uint8_t prg_ram_protect;  // $A001
    uint8_t irq_latch;       // $C000
    uint8_t irq_counter;      // $C001 reload
    bool irq_enabled;         // $E001 enable, $E000 disable
    int prg_mode;           // 0/1 (bank position swap)
    int chr_mode;           // 0/1 (2KB/1KB CHR bank mode)
};
```

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/cartridge/mappers.md` - Mapper 参考
- `docs/hardware/cartridge/rom-format.md` - iNES 格式参考
- `docs/hardware/memory/memory-map.md` - 内存映射参考

## 变更记录 (Change History)

- 2026-04-30: Initial version
