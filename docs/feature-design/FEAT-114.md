# FEAT-114: 精彩回放

## 元数据 (Metadata)

- **ID**: FEAT-114
- **关联模块 (Related Module)**: MOD-REPLAY
- **关联需求 (Related Requirements)**: REQ-114
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30

## 功能描述 (Feature Description)

环形缓冲区录制 + 自动高光检测 + 一键生成短视频。

## 接口定义 (Interface Definition)

```cpp
// 录制器
class ReplayRecorder {
public:
    void init(int buffer_seconds = 60);  // 环形缓冲区时长
    void push_frame(const FrameData& frame);
    void set_enabled(bool e) { enabled_ = e; }

    // 高光
    void mark_highlight(HighlightType type, const std::string& desc);
    std::vector<Highlight> get_highlights() const;

    // 生成视频
    bool export_clip(const Highlight& hl, const std::string& output);
    bool export_last_n_seconds(int seconds, const std::string& output);

private:
    RingBuffer<FrameData> buffer_;
    std::vector<Highlight> highlights_;
    HighlightDetector detector_;
    VideoGenerator generator_;
    bool enabled_ = true;
};
```

## 流程图 (Flow Chart)

```
[Emulator Running]
    → [Every frame: push to ring buffer]
        → [Highlight Detector: check conditions]
            → [If highlight: save timestamp + type]
                → [User presses 'Save Replay' or auto-save]
                    → [Extract frames from buffer around highlight]
                        → [VideoGenerator: encode to MP4]
                            → [Save to file: highlight_YYYYMMDD_HHMMSS.mp4]
```

## 边界条件 (Edge Cases)

1. **缓冲区满**：最旧的帧被覆盖（环形缓冲区）
2. **高光重叠**：两个高光靠得很近时合并
3. **手动标记**：用户标记时如果正在录制则保存
4. **编码失败**：磁盘满/编码错误时提示用户
5. **无音频**：只录制视频轨

## 测试场景 (Test Scenarios)

1. **环形录制**：60 秒缓冲区正确循环覆盖
2. **自动高光**：1UP/通关/BOSS 击杀自动检测
3. **手动标记**：用户按快捷键正确标记
4. **视频生成**：MP4 文件正确生成（H.264 + AAC）
5. **回放速度**：慢动作/快进正确
6. **分享功能**：至少支持 1 个平台分享
7. **缓冲区调整**：30s/60s/300s 切换正确
8. **性能**：录制帧率下降 ≤ 10%
9. **多高光**：一次游戏识别多个高光时刻
10. **格式支持**：MP4/WebM 输出可选

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/ppu/rendering.md`
- `docs/hardware/apu/audio-channels.md`

## 变更记录 (Change History)

- 2026-04-30: Initial version
