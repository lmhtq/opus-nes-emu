# FEAT-004: 内存映射与 Mapper 实现#

## 元数据 (Metadata)

- **ID**: FEAT-004
- **关联模块 (Related Module)**: MOD-MEMORY, MOD-CARTRIDGE
- **关联需求 (Related Requirements)**: REQ-004
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30

## 功能描述 (Feature Description)

实现 CPU 地址空间映射和 5 种 Mapper（0/1/2/3/4）。

## 接口定义 (Interface Definition)

```cpp
class Memory {
public:
    uint8_t read(uint16_t addr);
    void write(uint16_t addr, uint8_t val);
};

class Mapper {
public:
    virtual uint8_t cpu_read(uint16_t addr) = 0;
    virtual void cpu_write(uint16_t addr, uint8_t val) = 0;
};
```

## 流程图 (Flow Chart)

```
[CPU Read/Write addr]
    → [If addr < $2000: return RAM[mirror(addr)]]
        → [If $2000 <= addr < $4000: return PPU register]
            → [If $4000 <= addr < $4020: return APU register]
                → [If addr >= $4020: call Mapper::read/write]
```

## 边界条件 (Edge Cases)

1. **iNES 头部损坏**：拒绝加载
2. **Mapper 不支持**：提示用户
3. **电池 RAM**：正确保存/加载 .sav 文件
4. **Mapper 4 IRQ**：扫描线计数正确
5. **镜像**：水平/垂直/四屏幕正确

## 测试场景 (Test Scenarios)

1. RAM 读写+镜像正确
2. PPU/APU 寄存器读写
3. iNES 头部解析正确
4. Mapper 0：16KB/32KB PRG 映射
5. Mapper 1：串口写入+银行切换
6. Mapper 2：$8000-$BFFF 银行切换
7. Mapper 3：CHR ROM 切换
8. Mapper 4：多银行+IRQ
9. 电池 RAM 保存/加载
10. Mapper 4 IRQ 触发正确

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/memory/memory-map.md`
- `docs/hardware/cartridge/mappers.md`
- `docs/hardware/cartridge/rom-format.md`

## 变更记录 (Change History)

- 2026-04-30: Initial version
