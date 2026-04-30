# FEAT-112: RGB 灯光同步#

## 元数据 (Metadata)

- **ID**: FEAT-112
- **关联模块 (Related Module)**: MOD-HAPTICS
- **关联需求 (Related Requirements)**: REQ-112
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30#

## 功能描述 (Feature Description)

根据游戏内容改变手柄/键盘 RGB 灯光。

## 接口定义 (Interface Definition)

```cpp
class HapticsManager {
public:
    void set_light_for_health(int health_percent);
    void on_item_collect();
    void on_low_health();
    void set_light_preset(LightMode mode);
};
```

## 流程图 (Flow Chart)

```
[Game Running]
    → [ResourceAnalyzer: read health, items, scene]
        → [If health < 30%:]
            → [Set RGB: pulse red]
                → [If item collected:]
                    → [Flash green for 1s]
                        → [Update RGB device]
```

## 边界条件 (Edge Cases)

1. **RGB 设备不可用**：跳过
2. **多 RGB 设备**：广播到所有设备
3. **灯光预设冲突**：后设置的覆盖
4. **实时开关**：不影响游戏

## 测试场景 (Test Scenarios)

1. 血量感知正确（< 30% 红色脉冲）
2. 道具收集闪烁（绿色 1-2s）
3. 受伤反馈正确（红色闪烁）
4. 场景氛围灯光（至少 5 种）
5. 主流 RGB 设备识别
6. 灯光配置格式清晰
7. 至少 3 种预设模式
8. 实时开关
9. 多设备同步
10. 性能影响小

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/input/controllers.md`

## 变更记录 (Change History)

- 2026-04-30: Initial version
