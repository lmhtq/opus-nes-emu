# MOD-AUDIO: 音质增强

## 元数据 (Metadata)

- **ID**: MOD-AUDIO
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-106, REQ-107, REQ-108, REQ-109, REQ-110
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 功能职责 (Responsibilities)

实现音质增强功能，让 FC 游戏音频更具现代感。

核心职责：
1. 立体声扩展（单声道 → 立体声）
2. 3D 空间音频
3. 10 段图形均衡器
4. 混响效果（房间混响模拟）
5. 低音/高音增强
6. 音量标准化
7. 动态音效联动（场景自适应）
8. 音频可视化（波形/频谱）
9. 音频替换系统（remix 音乐/音效）
10. 预制音频包管理

## 接口设计 (Interface Design)

```cpp
// include/fcemu/audio_enhancement.h
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <array>

namespace fcemu {

// 均衡器频段
struct EqualizerBands {
    float band[10];  // 32Hz, 64Hz, 125Hz, 250Hz, 500Hz, 1KHz, 2KHz, 4KHz, 8KHz, 16KHz
};

// 混响参数
struct ReverbParams {
    float room_size = 0.5f;     // 0-1
    float damping = 0.5f;       // 0-1
    float wet_mix = 0.3f;      // 0-1
    float dry_mix = 0.7f;      // 0-1
    bool enabled = false;
};

// 立体声扩展
struct StereoExpand {
    float width = 0.5f;  // 0=mono, 1=max width
    bool enabled = false;
};

// 音频增强器
class AudioEnhancer {
public:
    AudioEnhancer();

    bool init(int sample_rate = 44100);
    void shutdown();

    // 处理音频采样（输入 NES APU 单声道采样，输出立体声）
    void process_samples(const std::vector<int16_t>& input,
                       std::vector<int16_t>& output);

    // 均衡器
    void set_equalizer(const EqualizerBands& bands);
    const EqualizerBands& equalizer() const { return eq_bands_; }

    // 混响
    void set_reverb(const ReverbParams& params);
    const ReverbParams& reverb() const { return reverb_; }

    // 立体声扩展
    void set_stereo_expand(const StereoExpand& expand);
    const StereoExpand& stereo_expand() const { return stereo_; }

    // 低音/高音增强
    void set_bass_boost(float db);   // -12 to +12
    void set_treble_boost(float db);  // -12 to +12

    // 音量标准化
    void set_normalization(bool enable, float target_db = -3.0f);

    // 音频替换
    bool load_remix_track(const std::string& track_name, const std::string& file_path);
    void enable_remix(bool enable);
    void set_remix_volume(float vol);  // 0-1

    // 动态音效联动
    void set_scene(const std::string& scene);  // "boss", "combat", "explore", "menu"
    void register_scene_params(const std::string& scene, const EqualizerBands& eq,
                             const ReverbParams& reverb, float bass, float treble);

    // 音频可视化
    struct VisualizationData {
        std::vector<float> waveform;   // 时域
        std::vector<float> spectrum;   // 频域（FFT）
    };
    VisualizationData get_visualization() const;

    // 预制音频包
    bool load_preset_pack(const std::string& manifest_path);

private:
    int sample_rate_;
    EqualizerBands eq_bands_;
    ReverbParams reverb_;
    StereoExpand stereo_;
    float bass_boost_ = 0.0f;
    float treble_boost_ = 0.0f;
    bool normalization_ = false;

    // 音频替换
    std::map<std::string, std::string> remix_tracks_;
    bool remix_enabled_ = false;

    // 动态场景
    std::string current_scene_;
    struct SceneParams {
        EqualizerBands eq;
        ReverbParams reverb;
        float bass;
        float treble;
    };
    std::map<std::string, SceneParams> scene_params_;

    // 可视化
    VisualizationData vis_data_;
    void update_visualization(const std::vector<int16_t>& samples);

    // 内部 DSP
    void apply_equalizer(std::vector<float>& samples);
    void apply_reverb(std::vector<float>& left, std::vector<float>& right);
    void apply_stereo_expand(std::vector<float>& left, std::vector<float>& right);
};

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-APU | 获取原始 APU 采样输出 |
| MOD-UI | 音频设置界面 |
| MOD-RESOURCE | 获取可替换的音频资源 |
| MOD-PRESETS | 加载预制音频包 |

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/apu/registers.md` - APU 寄存器
- `docs/hardware/apu/audio-channels.md` - 音频通道

## 变更记录 (Change History)

- 2026-04-30: Initial version
