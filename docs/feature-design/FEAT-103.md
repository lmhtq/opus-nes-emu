# FEAT-103: 宽屏补丁

## 元数据 (Metadata)

- **ID**: FEAT-103
- **关联模块 (Related Module)**: MOD-VIDEO
- **关联需求 (Related Requirements)**: REQ-103
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30

## 功能描述 (Feature Description)

将 FC 游戏从 4:3 扩展到 16:9，动态扩展画面边缘。

## 接口定义 (Interface Definition)

```cpp
struct WidescreenConfig {
    std::string game_name;
    uint16_t nametable_base;      // 主名称表基地址
    uint16_t nametable_extend;    // 扩展名称表基地址
    int left_extend;              // 左侧扩展像素数
    int right_extend;             // 右侧扩展像素数
    bool horizontal_mirror;        // 扩展区域镜像模式
};

class WidescreenEnhancer {
public:
    bool load_config(const std::string& game_name, uint16_t nametable_base);
    void render_widescreen(const FrameBuffer& original,
                             FrameBuffer& output);
    int get_output_width() const { return 426; }  // 256 * 16/9 = 426
    bool enabled() const { return enabled_; }
    void set_enabled(bool e) { enabled_ = e; }
};
```

## 流程图 (Flow Chart)

```
[Original 256x240 Frame]
    → [Calculate widescreen width: 426 pixels]
        → [For x = 0 to 425]:
            → [If x < 256: use original pixel]
                → [If x >= 256: read from adjacent nametable]
                    → [Composite: original + extended]
                        → [Output 426x240 Widescreen Frame]
```

## 边界条件 (Edge Cases)

1. **名称表边界**：扩展区域从相邻名称表读取
2. **不支持的游戏**：显示黑边而非拉伸
3. **镜像模式**：水平镜像时扩展区域正确镜像
4. **滚动**：水平滚动时扩展区域跟随
5. **精灵位置**：精灵在扩展区域正确显示/裁剪

## 测试场景 (Test Scenarios)

1. **16:9 渲染**：正确渲染 426x240 画面，无拉伸
2. **扩展区域**：从相邻名称表正确读取
3. **水平滚动**：滚动时宽屏区域正确更新
4. **精灵显示**：精灵在扩展区域正确显示
5. **游戏数据库**：至少 10 个游戏有宽屏配置
6. **手动配置**：用户自定义扩展区域，JSON 格式
7. **原始/宽屏切换**：实时切换，画面正确更新
8. **不支持游戏**：显示黑边，不崩溃
9. **垂直滚动**：垂直滚动不受影响
10. **性能**：宽屏渲染帧率下降 ≤ 5%

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/ppu/rendering.md`
- `docs/hardware/memory/memory-map.md`

## 变更记录 (Change History)

- 2026-04-30: Initial version
