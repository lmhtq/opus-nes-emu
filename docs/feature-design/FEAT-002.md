# FEAT-002: PPU 渲染管线

## 元数据 (Metadata)

- **ID**: FEAT-002
- **关联模块 (Related Module)**: MOD-PPU
- **关联需求 (Related Requirements)**: REQ-002
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30

## 功能描述 (Feature Description)

实现 PPU 的完整渲染管线，包括背景层、精灵层、调色板系统、扫描线时序。

## 接口定义 (Interface Definition)

```cpp
class Ppu {
public:
    void step(int cpu_cycles);  // 每 3 个 CPU 周期 = 1 个 PPU 周期
    const FrameBuffer& get_frame() const;

    // 背景渲染
    void render_background(int scanline);
    uint16_t get_nametable_addr(int scanline, int dot);
    uint8_t get_attribute(int scanline, int dot);

    // 精灵渲染
    void evaluate_sprites(int scanline);
    void render_sprites(int scanline);

    // 调色板
    uint8_t read_palette(uint16_t addr);
    void write_palette(uint16_t addr, uint8_t val);

    // 混叠（背景 + 精灵）
    uint8_t composite_pixel(int x, int y,
                                  uint8_t bg_pixel,
                                  uint8_t sprite_pixel,
                                  int sprite_idx);
};
```

## 流程图 (Flow Chart)

```
[For each scanline 0-261]:
    [For each dot 0-340]:
        → [Background: fetch nametable byte]
            → [Background: fetch attribute byte]
                → [Background: fetch tile low byte]
                    → [Background: fetch tile high byte]
                        → [Sprite: evaluate sprites for next scanline]
                            → [Composite: mix bg + sprite]
                                → [Output pixel to framebuffer]
```

## 边界条件 (Edge Cases)

1. **精灵 0 命中**：背景和精灵 0 的非透明像素重叠时设置 $2002 bit 6
2. **精灵溢出**：同一扫描线超过 8 个精灵时设置 $2002 bit 5
3. **左侧 8 像素隐藏**：当 $2001 bit 1/2 = 0 时隐藏
4. **调色板镜像**：$3F00/$3F04/$3F08/$3F0C 是通用背景色
5. **精灵优先级**：高 OAM 索引优先，除非属性 bit 5=1（背景优先）
6. **翻转**：水平翻转（bit 6）、垂直翻转（bit 7）
7. **8x16 精灵**：使用两个连续的 Tile

## 测试场景 (Test Scenarios)

1. **背景渲染**：正确渲染 32x30 个 Tile（256x240 像素）
2. **精灵渲染**：64 个精灵，每扫描线最多 8 个
3. **调色板**：正确映射 56 种颜色
4. **精灵 0 碰撞**：碰撞时正确设置标志位
5. **精灵溢出**：超过 8 个/扫描线时设置溢出标志
6. **滚动**：水平和垂直滚动正确
7. **VBlank**：扫描线 241 设置 $2002 bit 7，触发 NMI
8. **镜像**：水平/垂直/四屏幕镜像正确
9. **精灵翻转**：水平和垂直翻转正确
10. **优先级**：精灵和背景的优先级正确

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/ppu/registers.md`
- `docs/hardware/ppu/rendering.md`
- `docs/hardware/ppu/sprites.md`

## 变更记录 (Change History)

- 2026-04-30: Initial version
