# FEAT-113: 直播互动模式

## 元数据 (Metadata)

- **ID**: FEAT-113
- **关联模块 (Related Module)**: MOD-SOCIAL
- **关联需求 (Related Requirements)**: REQ-113
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30

## 功能描述 (Feature Description)

实现直播互动模式，弹幕、投票、刷礼物触发游戏事件。

## 接口定义 (Interface Definition)

```cpp
// 游戏事件注入
class GameEventInjector {
public:
    void inject_event(const GameEvent& event);
    // 通过写入 CPU 内存或直接修改寄存器来实现
    // 例如：加命 → 写入 $075A（SMB 的命数地址）
private:
    class Cpu6502* cpu_;
    class Memory* memory_;
    std::map<std::string, uint16_t> game_addresses_;
};

// OBS 数据输出
struct ObsOutput {
    int viewer_count;
    int danmu_count;
    int gift_total;
    std::string vote_result;
    std::string current_game;
};
```

## 流程图 (Flow Chart)

```
[Live Platform: Bilibili/Douyu/Twitch]
    → [WebSocket/HTTP Poll: fetch danmu/gifts]
        → [Parse: extract text, user, gift_value]
            → [Match: compare with event mappings]
                → [If match: create GameEvent]
                    → [Inject into emulator: modify memory/trigger NMI]
                        → [Game reacts: add life/item/effect]
                            → [Update OBS panel: show stats]
```

## 边界条件 (Edge Cases)

1. **礼物价值**：不同价值对应不同道具（如 1 鱼丸=蘑菇，10 鱼丸=火焰花）
2. **弹幕风暴**：大量弹幕时限制处理频率（如最多 100 条/秒）
3. **投票平局**：随机选择一个选项
4. **无效映射**：跳过无法识别的弹幕/礼物
5. **断线重连**：自动重连直播平台

## 测试场景 (Test Scenarios)

1. **弹幕触发**：发送"蘑菇"弹幕 → 游戏获得蘑菇道具
2. **投票功能**：观众投票二选一 → 游戏执行选择
3. **刷礼物**：送 10 鱼丸 → 触发火焰花道具
4. **OBS 面板**：数据正确显示，可控制开关
5. **平台对接**：至少支持 2 个直播平台
6. **配置文件**：事件映射 JSON 格式正确
7. **实时开关**：直播模式可实时开关
8. **性能影响**：帧率下降 ≤ 5%
9. **断线处理**：网络断开后自动重连
10. **统计面板**：观众数/弹幕数/礼物数准确

## 关联硬件文档 (Related Hardware Docs)

- 无（直播互动是模拟器扩展功能）

## 变更记录 (Change History)

- 2026-04-30: Initial version
