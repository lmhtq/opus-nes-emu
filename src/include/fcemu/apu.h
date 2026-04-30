// include/fcemu/apu.h
#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace fcemu {

enum class ApuChannel { Pulse1, Pulse2, Triangle, Noise, Dmc };

using AudioSampleCallback = std::function<void(const std::vector<int16_t>&)>;
using DmcDmaReader        = std::function<uint8_t(uint16_t)>;

class Apu {
public:
    Apu();
    bool init(int sample_rate);
    void reset();

    void set_sample_callback(AudioSampleCallback cb) { sample_cb_ = std::move(cb); }
    void set_sample_rate(int rate);
    void set_dmc_reader(DmcDmaReader r) { dmc_reader_ = std::move(r); }

    uint8_t cpu_read(uint16_t addr);
    void    cpu_write(uint16_t addr, uint8_t val);

    void step(int cpu_cycles);
    void generate_samples(int num_samples); // explicit pull (optional)

    int16_t get_channel_output(ApuChannel ch) const;
    bool    irq_pending() const { return frame_irq_ || dmc_.irq_flag; }

    void serialize(class Serializer& s) const;
    void deserialize(class Deserializer& d);

private:
    struct Pulse {
        bool   enabled = false;
        bool   length_halt = false;
        bool   constant_vol = false;
        uint8_t volume = 0;
        uint8_t duty = 0;
        // sweep
        bool   sweep_enabled = false;
        uint8_t sweep_period = 0;
        bool   sweep_negate = false;
        uint8_t sweep_shift = 0;
        bool   sweep_reload = false;
        uint8_t sweep_div = 0;
        // timer
        uint16_t timer = 0;
        uint16_t timer_counter = 0;
        uint8_t  seq_pos = 0;
        uint8_t  length = 0;
        // envelope
        bool    env_start = false;
        uint8_t env_div = 0;
        uint8_t env_decay = 0;
        bool    is_pulse2 = false;
        uint8_t output() const;
        bool    sweep_muting() const;
    };
    struct Triangle {
        bool   enabled = false;
        bool   length_halt = false;
        uint8_t linear_reload = 0;
        bool   linear_reload_flag = false;
        uint8_t linear_counter = 0;
        uint16_t timer = 0;
        uint16_t timer_counter = 0;
        uint8_t  seq_pos = 0;
        uint8_t  length = 0;
        uint8_t output() const;
    };
    struct Noise {
        bool   enabled = false;
        bool   length_halt = false;
        bool   constant_vol = false;
        uint8_t volume = 0;
        bool   mode = false;
        uint16_t timer_period = 0;
        uint16_t timer_counter = 0;
        uint16_t shift = 1;
        uint8_t length = 0;
        bool    env_start = false;
        uint8_t env_div = 0;
        uint8_t env_decay = 0;
        uint8_t output() const;
    };
    struct Dmc {
        bool    enabled = false;
        bool    irq_enable = false;
        bool    loop = false;
        uint16_t rate = 0;
        uint16_t timer = 0;
        uint16_t sample_addr = 0;
        uint16_t sample_len = 0;
        uint16_t cur_addr = 0;
        uint16_t bytes_left = 0;
        uint8_t  shift = 0;
        uint8_t  bits_left = 0;
        uint8_t  output_level = 0;
        bool     silence = true;
        bool     sample_buffer_filled = false;
        uint8_t  sample_buffer = 0;
        bool     irq_flag = false;
    };

    Pulse    pulse1_, pulse2_;
    Triangle triangle_;
    Noise    noise_;
    Dmc      dmc_;

    int      sample_rate_ = 44100;
    double   cycles_per_sample_ = 0;
    double   sample_accum_ = 0;
    uint64_t total_cycles_ = 0;

    // Frame counter
    int      frame_step_ = 0;
    bool     frame_5step_ = false;
    bool     frame_irq_inhibit_ = false;
    bool     frame_irq_ = false;
    int      frame_div_ = 0;

    std::vector<int16_t> sample_buffer_;
    AudioSampleCallback  sample_cb_;
    DmcDmaReader         dmc_reader_;

    void clock_quarter_frame();
    void clock_half_frame();
    void clock_envelopes();
    void clock_length_counters();
    void clock_sweeps();
    void clock_timers_cpu();
    void emit_sample();
    int16_t mix() const;
};

} // namespace fcemu
