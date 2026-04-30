// include/fcemu/apu.h
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace fcemu {

enum class ApuChannel { Pulse1, Pulse2, Triangle, Noise, Dmc };

struct ApuState {
    bool pulse1_enabled;
    bool pulse2_enabled;
    bool triangle_enabled;
    bool noise_enabled;
    bool dmc_enabled;
    uint8_t frame_mode;
};

using AudioSampleCallback = std::function<void(const std::vector<int16_t>&)>;

class Apu {
public:
    Apu();
    bool init(int sample_rate);
    void reset();
    void set_sample_callback(AudioSampleCallback callback);
    void set_sample_rate(int rate);
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t val);
    void step(int cpu_cycles);
    void generate_samples(int num_samples);
    int16_t get_channel_output(ApuChannel ch) const;
    void request_dmc_dma(uint16_t addr);
    void clock_frame_counter();

private:
    struct PulseChannel {
        uint8_t control, sweep, timer_low, timer_high;
        int timer_, envelope_timer_, envelope_volume_;
        int sweep_timer_, length_counter_;
        bool sweep_reload_;
        int16_t output;
    };
    struct TriangleChannel {
        uint8_t control, timer_low, timer_high;
        int timer_, linear_counter_, length_counter_;
        int16_t output;
    };
    struct NoiseChannel {
        uint8_t control, mode, length;
        int timer_, length_counter_;
        uint16_t shift_register_;
        int16_t output;
    };
    struct DmcChannel {
        uint8_t control, direct, address, length;
        int16_t output;
        uint16_t sample_addr_, sample_length_;
        int sample_buffer_, bits_remaining_;
        bool irq_flag_;
    };

    PulseChannel pulse1_, pulse2_;
    TriangleChannel triangle_;
    NoiseChannel noise_;
    DmcChannel dmc_;
    int frame_step_, frame_mode_;
    bool irq_disable_, frame_irq_flag_;
    int sample_rate_;
    std::vector<int16_t> sample_buffer_;
    AudioSampleCallback sample_callback_;

    int16_t mix_output() const;
    void clock_length_counters();
    void clock_envelope_sweep();
};

} // namespace fcemu
