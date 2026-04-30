# MOD-REPLAY: 精彩回放#

## 元数据 (Metadata)

- **ID**: MOD-REPLAY
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-114
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 功能职责 (Responsibilities)

实现即时精彩回放功能。

核心职责：
1. 环形缓冲区录制（画面 + 音频）
2. 自动检测高光时刻（1UP/通关/BOSS 击杀/隐藏道具）
3. 一键生成短视频（MP4）
4. 手动标记精彩时刻
5. 回放速度控制（慢动作/快进）
6. 短视频编辑（裁剪/滤镜/配乐）
7. 分享到社交平台

## 接口设计 (Interface Design)

```cpp
// include/fcemu/replay.h
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <functional>

namespace fcemu {

// 帧数据（包含画面和音频）
struct FrameData {
    std::vector<uint8_t> video;  // RGBA 256x240
    std::vector<int16_t> audio; // 音频采样
    uint64_t timestamp;
};

// 高光类型
enum class HighlightType {
    OneUp,       // 1UP
    LevelClear,   // 通关
    BossKill,     // BOSS 击杀
    HiddenItem,    // 隐藏道具
    Custom        // 用户手动标记
};

// 高光时刻
struct Highlight {
    HighlightType type;
    uint64_t timestamp;
    std::string description;
    int duration_ms = 5000;  // 高光前后时长
};

// 回放缓冲区
class ReplayBuffer {
public:
    ReplayBuffer();

    void init(int max_duration_seconds = 60);  // 默认 60 秒环形缓冲
    void shutdown();

    // 添加帧
    void push_frame(const FrameData& frame);

    // 获取高光片段
    std::vector<FrameData> get_highlight_clip(const Highlight& hl) const;

    // 获取最近 N 秒
    std::vector<FrameData> get_last_n_seconds(int seconds) const;

    // 缓冲区状态
    int current_duration_ms() const;
    int max_duration_ms() const { return max_duration_ms_; }
    bool is_full() const;

private:
    std::vector<FrameData> buffer_;
    int max_duration_ms_;
    size_t write_pos_ = 0;
    bool wrapped_ = false;
};

// 高光检测器
class HighlightDetector {
public:
    HighlightDetector();

    void set_callbacks(class Cpu6502* cpu, class Ppu* ppu, class Apu* apu);

    // 检测高光
    bool check_highlight(HighlightType type, Highlight& out) const;

    // 注册自定义检测
    using DetectCallback = std::function<bool()>;
    void register_detector(HighlightType type, DetectCallback cb);

private:
    Cpu6502* cpu_ = nullptr;
    Ppu* ppu_ = nullptr;
    Apu* apu_ = nullptr;

    // 内置检测器
    bool detect_1up() const;
    bool detect_boss_kill() const;
    bool detect_hidden_item() const;
    bool detect_level_clear() const;
};

// 视频生成器
class VideoGenerator {
public:
    VideoGenerator();

    bool init(int width, int height, int fps);

    // 生成视频
    bool generate_mp4(const std::vector<FrameData>& frames,
                       const std::string& output_path);

    // 添加滤镜
    enum class Filter { None, Vintage, Vibrant, Dramatic };
    void set_filter(Filter f) { filter_ = f; }

    // 添加背景音乐
    void set_background_music(const std::string& music_path);

private:
    int width_, height_, fps_;
    Filter filter_ = Filter::None;
    std::string bg_music_;
};

// 回放管理器
class ReplayManager {
public:
    ReplayManager();

    bool init();
    void shutdown();

    // 录制控制
    void start_recording();
    void stop_recording();
    bool recording() const { return recording_; }

    // 高光管理
    void check_and_save_highlights();
    const std::vector<Highlight>& highlights() const { return highlights_; }

    // 手动标记
    void mark_highlight(HighlightType type, const std::string& desc);

    // 生成短视频
    bool generate_clip(const Highlight& hl, const std::string& output);
    bool generate_last_n_seconds(int seconds, const std::string& output);

    // 分享
    bool share_to_platform(const std::string& clip_path, const std::string& platform);

    // 设置
    void set_buffer_duration(int seconds);
    void set_auto_detect(bool enable) { auto_detect_ = enable; }

private:
    ReplayBuffer buffer_;
    HighlightDetector detector_;
    VideoGenerator generator_;
    std::vector<Highlight> highlights_;
    bool recording_ = false;
    bool auto_detect_ = true;
};

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-CPU | 检测高光（如 1UP 指令） |
| MOD-PPU | 获取画面数据 |
| MOD-APU | 获取音频数据 |
| MOD-UI | 回放控制界面 |

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/ppu/rendering.md` - 画面数据
- `docs/hardware/apu/audio-channels.md` - 音频数据

## 变更记录 (Change History)

- 2026-04-30: Initial version
