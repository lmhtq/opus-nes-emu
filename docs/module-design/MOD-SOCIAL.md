# MOD-SOCIAL: 直播互动#

## 元数据 (Metadata)

- **ID**: MOD-SOCIAL
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-113
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 功能职责 (Responsibilities)

实现直播互动模式，让观众参与游戏过程。

核心职责：
1. 弹幕关键词检测（触发游戏事件）
2. 观众投票系统（二选一剧情/操作）
3. 礼物价值映射（不同礼物对应不同道具/特效）
4. 平台对接（Bilibili/斗鱼/虎牙/Twitch 等）
5. OBS 插件面板（主播控制）
6. 互动统计面板
7. 事件注入到模拟器（通过 CPU 内存写入）

## 接口设计 (Interface Design)

```cpp
// include/fcemu/social.h
#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <vector>
#include <map>

namespace fcemu {

// 弹幕事件
struct DanmuEvent {
    std::string user;
    std::string text;
    int gift_value = 0;  // 0 = 普通弹幕，>0 = 礼物价值
    int64_t timestamp;
};

// 投票选项
struct VoteOption {
    std::string text;
    int count = 0;
};

// 投票事件
struct VoteEvent {
    std::string title;
    std::vector<VoteOption> options;
    int duration_seconds = 30;
};

// 游戏事件（注入到模拟器）
struct GameEvent {
    enum class Type {
        AddLife,     // 加命
        AddItem,     // 加道具
        TriggerEffect, // 触发特效
        ModifySpeed,   // 修改速度
        Custom        // 自定义
    };
    Type type;
    std::string param1;
    std::string param2;
};

// 弹幕/礼物 → 游戏事件映射
struct EventMapping {
    std::string trigger;       // 关键词或礼物名
    GameEvent::Type event_type;
    std::string event_param1;
    std::string event_param2;
    int min_gift_value = 0;  // 最低礼物价值
};

// 直播平台接口
class LivePlatform {
public:
    virtual ~LivePlatform() = default;
    virtual bool connect(const std::string& room_id) = 0;
    virtual void disconnect() = 0;
    virtual bool connected() const = 0;
    virtual void update() = 0;  // 轮询弹幕
};

// 直播互动管理器
class SocialManager {
public:
    SocialManager();

    bool init();
    void shutdown();

    // 平台管理
    bool connect_platform(const std::string& platform, const std::string& room_id);
    void disconnect_platform();
    std::string current_platform() const { return platform_name_; }

    // 事件映射配置
    void load_mappings(const std::string& config_path);
    void add_mapping(const EventMapping& mapping);

    // 投票
    void start_vote(const VoteEvent& vote);
    void end_vote();
    const VoteEvent& current_vote() const { return current_vote_; }
    bool vote_in_progress() const { return vote_active_; }

    // 事件注入回调（连接到模拟器）
    using GameEventCallback = std::function<void(const GameEvent&)>;
    void set_game_event_callback(GameEventCallback cb);

    // OBS 面板数据
    struct ObsData {
        int viewer_count;
        int danmu_count;
        int gift_total_value;
        std::string current_vote_result;
    };
    ObsData get_obs_data() const;

    // 统计
    int total_danmu() const { return total_danmu_; }
    int total_gift_value() const { return total_gift_value_; }

private:
    // 弹幕处理
    void on_danmu(const DanmuEvent& event);
    void process_danmu_trigger(const std::string& text);
    void process_gift_trigger(const std::string& gift, int value);

    // 投票
    VoteEvent current_vote_;
    bool vote_active_ = false;
    int vote_timer_ = 0;

    // 平台
    std::unique_ptr<LivePlatform> platform_;
    std::string platform_name_;

    // 映射
    std::vector<EventMapping> mappings_;

    // 回调
    GameEventCallback game_event_cb_;

    // 统计
    int total_danmu_ = 0;
    int total_gift_value_ = 0;
};

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-CPU | 通过 CPU 内存写入注入游戏事件 |
| MOD-UI | OBS 面板、设置界面 |

## 关联硬件文档 (Related Hardware Docs)

- 无（直播互动是模拟器扩展功能）

## 变更记录 (Change History)

- 2026-04-30: Initial version
