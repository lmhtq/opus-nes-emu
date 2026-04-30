# MOD-APU: APU 音频模拟器

## 元数据 (Metadata)

- **ID**: MOD-APU
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-003
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 功能职责 (Responsibilities)

模拟 FC/NES 的 APU（Audio Processing Unit）。

核心职责：
1. 实现 2 个脉冲波（Pulse/Square）通道
2. 实现 1 个三角波（Triangle）通道
3. 实现 1 个噪声（Noise）通道
4. 实现 1 个 DMC（Delta Modulation）通道
5. 实现帧计数器（Frame Counter，$4017）
6. 实现各通道的长度计数器
7. 实现脉冲波的包络和扫频
8. 实现三角波的线性计数器
9. 混音输出（5 通道混合）
10. 支持 DMC DMA 采样播放

## 接口设计 (Interface Design)

```cpp
// include/fcemu/apu.h
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace fcemu {

// APU 通道类型
enum class ApuChannel {
    Pulse1,
    Pulse2,
    Triangle,
    Noise,
    Dmc
};

// APU 状态
struct ApuState {
    bool pulse1_enabled;
    bool pulse2_enabled;
    bool triangle_enabled;
    bool noise_enabled;
    bool dmc_enabled;
    uint8_t frame_mode;  // $4017 模式
};

// 音频采样回调（输出采样）
using AudioSampleCallback = std::function<void(const std::vector<int16_t>& samples)>;

class Apu {
public:
    Apu();

    void reset();
    void set_sample_callback(AudioSampleCallback callback);
    void set_sample_rate(int rate);  // 输出采样率（如 44100）

    // CPU 侧寄存器访问
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t val);

    // 执行一个 APU 周期（或一步帧计数器）
    void step(int cpu_cycles);

    // 生成音频采样（到输出缓冲区）
    void generate_samples(int num_samples);

    // 获取单个通道输出（用于调试/可视化）
    int16_t get_channel_output(ApuChannel ch) const;

    // DMC DMA 请求（由 CPU 触发）
    void request_dmc_dma(uint16_t addr);

    // 帧计数器步进
    void clock_frame_counter();

private:
    // 脉冲波通道
    struct PulseChannel {
        uint8_t control;       // $4000/$4004
        uint8_t sweep;          // $4001/$4005
        uint8_t timer_low;      // $4002/$4006
        uint8_t timer_high;     // $4003/$4007
        int16_t output;
        // 内部状态
        int timer_;
        int envelope_timer_;
        int envelope_volume_;
        int sweep_timer_;
        int length_counter_;
        bool sweep_reload_;
    };

    // 三角波通道
    struct TriangleChannel {
        uint8_t control;       // $4008
        uint8_t timer_low;      // $400A
        uint8_t timer_high;     // $400B
        int16_t output;
        // 内部状态
        int timer_;
        int linear_counter_;
        int length_counter_;
    };

    // 噪声通道
    struct NoiseChannel {
        uint8_t control;       // $400C
        uint8_t mode;          // $400E
        uint8_t length;        // $400F
        int16_t output;
        int timer_;
        int length_counter_;
        uint16_t shift_register_;
    };

    // DMC 通道
    struct DmcChannel {
        uint8_t control;       // $4010
        uint8_t direct;        // $4011
        uint8_t address;       // $4012
        uint8_t length;        // $4013
        int16_t output;
        // 内部状态
        uint16_t sample_addr_;
        uint16_t sample_length_;
        int sample_buffer_;
        int bits_remaining_;
        bool irq_flag_;
    };

    PulseChannel pulse1_;
    PulseChannel pulse2_;
    TriangleChannel triangle_;
    NoiseChannel noise_;
    DmcChannel dmc_;

    // 帧计数器
    int frame_step_;
    int frame_mode_;
    bool irq_disable_;
    bool frame_irq_flag_;

    // 输出
    int sample_rate_;
    std::vector<int16_t> sample_buffer_;
    AudioSampleCallback sample_callback_;

    // 辅助函数
    int16_t mix_output() const;
    void clock_length_counters();
    void clock_envelope_sweep();
};

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-CPU | APU 寄存器被 CPU 通过 $4000-$4017 访问 |
| MOD-MEMORY | DMC 通道从内存读取采样数据 |

## 数据结构 (Data Structures)

### 脉冲波占空比

```cpp
// control 寄存器 bit 6-7
// 00 = 12.5%, 01 = 25%, 10 = 50%, 11 = 75%
constexpr int DUTY_CYCLES[4] = {1, 2, 4, 6};  // 在 8 步序列中的位置
```

### 帧计数器模式

```cpp
// frame_mode_ = 0: 4 步模式
// step 0: clock length + sweep
// step 1: clock envelope
// step 2: clock length + sweep
// step 3: clock envelope + IRQ
//
// frame_mode_ = 1: 5 步模式
// step 0: clock envelope
// step 1: clock length + sweep
// step 2: clock envelope
// step 3: clock length + sweep
// step 4: clock envelope
```

## 状态机 (State Machines)

### 帧计数器

```
[4-Step Mode]
Step 0 (~3728.5 cycles) → Clock Length + Sweep
    → Step 1 (~3728.5 cycles) → Clock Envelope
        → Step 2 (~3728.5 cycles) → Clock Length + Sweep
            → Step 3 (~3728.5 cycles) → Clock Envelope + IRQ
                → back to Step 0
```

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/apu/registers.md` - APU 寄存器参考
- `docs/hardware/apu/audio-channels.md` - 音频通道参考

## 变更记录 (Change History)

- 2026-04-30: Initial version
