// apu.cpp - 5-channel APU synthesis (Pulse1/2, Triangle, Noise, DMC).
// References: nesdev wiki APU pages.
#include "fcemu/apu.h"

#include <algorithm>
#include <cmath>

namespace fcemu {

static const uint8_t LENGTH_TABLE[32] = {
    10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
    12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

static const uint8_t PULSE_DUTY[4][8] = {
    {0,1,0,0,0,0,0,0},
    {0,1,1,0,0,0,0,0},
    {0,1,1,1,1,0,0,0},
    {1,0,0,1,1,1,1,1},
};

static const uint8_t TRIANGLE_SEQ[32] = {
    15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15
};

static const uint16_t NOISE_PERIODS[16] = {
      4,    8,   16,   32,   64,   96,  128,  160,
    202,  254,  380,  508,  762, 1016, 2034, 4068
};

static const uint16_t DMC_RATES[16] = {
    428, 380, 340, 320, 286, 254, 226, 214,
    190, 160, 142, 128, 106,  84,  72,  54
};

uint8_t Apu::Pulse::output() const {
    if (!enabled || length == 0) return 0;
    if (timer < 8 || sweep_muting()) return 0;
    if (PULSE_DUTY[duty][seq_pos] == 0) return 0;
    return constant_vol ? volume : env_decay;
}

bool Apu::Pulse::sweep_muting() const {
    int delta = timer >> sweep_shift;
    int target = sweep_negate
        ? (int)timer - delta - (is_pulse2 ? 0 : 1)
        : (int)timer + delta;
    return target > 0x7FF;
}

uint8_t Apu::Triangle::output() const {
    if (!enabled || length == 0 || linear_counter == 0) return TRIANGLE_SEQ[seq_pos];
    return TRIANGLE_SEQ[seq_pos];
}

uint8_t Apu::Noise::output() const {
    if (!enabled || length == 0) return 0;
    if (shift & 1) return 0;
    return constant_vol ? volume : env_decay;
}

Apu::Apu() { reset(); }

bool Apu::init(int rate) {
    set_sample_rate(rate);
    reset();
    return true;
}

void Apu::set_sample_rate(int rate) {
    sample_rate_ = rate > 0 ? rate : 44100;
    cycles_per_sample_ = 1789773.0 / sample_rate_;
}

void Apu::reset() {
    pulse1_ = Pulse{};
    pulse2_ = Pulse{}; pulse2_.is_pulse2 = true;
    triangle_ = Triangle{};
    noise_ = Noise{}; noise_.shift = 1;
    dmc_ = Dmc{};
    frame_step_ = 0; frame_5step_ = false; frame_irq_inhibit_ = false;
    frame_irq_ = false; frame_div_ = 0;
    total_cycles_ = 0; sample_accum_ = 0;
    sample_buffer_.clear();
    if (cycles_per_sample_ <= 0) cycles_per_sample_ = 1789773.0 / sample_rate_;
}

uint8_t Apu::cpu_read(uint16_t addr) {
    if (addr == 0x4015) {
        uint8_t v = 0;
        if (pulse1_.length)  v |= 0x01;
        if (pulse2_.length)  v |= 0x02;
        if (triangle_.length) v |= 0x04;
        if (noise_.length)   v |= 0x08;
        if (dmc_.bytes_left) v |= 0x10;
        if (frame_irq_)      v |= 0x40;
        if (dmc_.irq_flag)   v |= 0x80;
        frame_irq_ = false;
        return v;
    }
    return 0;
}

void Apu::cpu_write(uint16_t addr, uint8_t val) {
    switch (addr) {
        case 0x4000:
            pulse1_.duty = (val >> 6) & 3;
            pulse1_.length_halt = val & 0x20;
            pulse1_.constant_vol = val & 0x10;
            pulse1_.volume = val & 0x0F;
            break;
        case 0x4001:
            pulse1_.sweep_enabled = val & 0x80;
            pulse1_.sweep_period = (val >> 4) & 7;
            pulse1_.sweep_negate = val & 0x08;
            pulse1_.sweep_shift = val & 0x07;
            pulse1_.sweep_reload = true;
            break;
        case 0x4002:
            pulse1_.timer = (pulse1_.timer & 0x0700) | val;
            break;
        case 0x4003:
            pulse1_.timer = (pulse1_.timer & 0x00FF) | ((val & 7) << 8);
            if (pulse1_.enabled) pulse1_.length = LENGTH_TABLE[(val >> 3) & 0x1F];
            pulse1_.seq_pos = 0;
            pulse1_.env_start = true;
            break;
        case 0x4004:
            pulse2_.duty = (val >> 6) & 3;
            pulse2_.length_halt = val & 0x20;
            pulse2_.constant_vol = val & 0x10;
            pulse2_.volume = val & 0x0F;
            break;
        case 0x4005:
            pulse2_.sweep_enabled = val & 0x80;
            pulse2_.sweep_period = (val >> 4) & 7;
            pulse2_.sweep_negate = val & 0x08;
            pulse2_.sweep_shift = val & 0x07;
            pulse2_.sweep_reload = true;
            break;
        case 0x4006:
            pulse2_.timer = (pulse2_.timer & 0x0700) | val;
            break;
        case 0x4007:
            pulse2_.timer = (pulse2_.timer & 0x00FF) | ((val & 7) << 8);
            if (pulse2_.enabled) pulse2_.length = LENGTH_TABLE[(val >> 3) & 0x1F];
            pulse2_.seq_pos = 0;
            pulse2_.env_start = true;
            break;
        case 0x4008:
            triangle_.length_halt = val & 0x80;
            triangle_.linear_reload = val & 0x7F;
            break;
        case 0x400A:
            triangle_.timer = (triangle_.timer & 0x0700) | val;
            break;
        case 0x400B:
            triangle_.timer = (triangle_.timer & 0x00FF) | ((val & 7) << 8);
            if (triangle_.enabled) triangle_.length = LENGTH_TABLE[(val >> 3) & 0x1F];
            triangle_.linear_reload_flag = true;
            break;
        case 0x400C:
            noise_.length_halt = val & 0x20;
            noise_.constant_vol = val & 0x10;
            noise_.volume = val & 0x0F;
            break;
        case 0x400E:
            noise_.mode = val & 0x80;
            noise_.timer_period = NOISE_PERIODS[val & 0x0F];
            break;
        case 0x400F:
            if (noise_.enabled) noise_.length = LENGTH_TABLE[(val >> 3) & 0x1F];
            noise_.env_start = true;
            break;
        case 0x4010:
            dmc_.irq_enable = val & 0x80;
            dmc_.loop = val & 0x40;
            dmc_.rate = DMC_RATES[val & 0x0F];
            if (!dmc_.irq_enable) dmc_.irq_flag = false;
            break;
        case 0x4011:
            dmc_.output_level = val & 0x7F;
            break;
        case 0x4012:
            dmc_.sample_addr = 0xC000 | ((uint16_t)val << 6);
            break;
        case 0x4013:
            dmc_.sample_len = ((uint16_t)val << 4) | 1;
            break;
        case 0x4015:
            pulse1_.enabled   = val & 0x01; if (!pulse1_.enabled)   pulse1_.length = 0;
            pulse2_.enabled   = val & 0x02; if (!pulse2_.enabled)   pulse2_.length = 0;
            triangle_.enabled = val & 0x04; if (!triangle_.enabled) triangle_.length = 0;
            noise_.enabled    = val & 0x08; if (!noise_.enabled)    noise_.length = 0;
            dmc_.enabled      = val & 0x10;
            if (!dmc_.enabled) {
                dmc_.bytes_left = 0;
            } else if (dmc_.bytes_left == 0) {
                dmc_.cur_addr = dmc_.sample_addr;
                dmc_.bytes_left = dmc_.sample_len;
            }
            dmc_.irq_flag = false;
            break;
        case 0x4017:
            frame_5step_ = val & 0x80;
            frame_irq_inhibit_ = val & 0x40;
            if (frame_irq_inhibit_) frame_irq_ = false;
            frame_div_ = 0;
            frame_step_ = 0;
            if (frame_5step_) { clock_quarter_frame(); clock_half_frame(); }
            break;
    }
}

void Apu::clock_envelopes() {
    auto step_env = [](bool& start, uint8_t& div, uint8_t& decay, uint8_t period, bool loop) {
        if (start) { start = false; decay = 15; div = period; }
        else if (div == 0) {
            div = period;
            if (decay) --decay;
            else if (loop) decay = 15;
        } else --div;
    };
    step_env(pulse1_.env_start, pulse1_.env_div, pulse1_.env_decay, pulse1_.volume, pulse1_.length_halt);
    step_env(pulse2_.env_start, pulse2_.env_div, pulse2_.env_decay, pulse2_.volume, pulse2_.length_halt);
    step_env(noise_.env_start,  noise_.env_div,  noise_.env_decay,  noise_.volume,  noise_.length_halt);

    if (triangle_.linear_reload_flag) triangle_.linear_counter = triangle_.linear_reload;
    else if (triangle_.linear_counter) --triangle_.linear_counter;
    if (!triangle_.length_halt) triangle_.linear_reload_flag = false;
}

void Apu::clock_length_counters() {
    if (!pulse1_.length_halt   && pulse1_.length)   --pulse1_.length;
    if (!pulse2_.length_halt   && pulse2_.length)   --pulse2_.length;
    if (!triangle_.length_halt && triangle_.length) --triangle_.length;
    if (!noise_.length_halt    && noise_.length)    --noise_.length;
}

void Apu::clock_sweeps() {
    auto step_sw = [](Pulse& p) {
        if (p.sweep_div == 0 && p.sweep_enabled && p.sweep_shift && !p.sweep_muting()) {
            int delta = p.timer >> p.sweep_shift;
            if (p.sweep_negate) p.timer -= delta + (p.is_pulse2 ? 0 : 1);
            else                p.timer += delta;
        }
        if (p.sweep_div == 0 || p.sweep_reload) { p.sweep_div = p.sweep_period; p.sweep_reload = false; }
        else --p.sweep_div;
    };
    step_sw(pulse1_); step_sw(pulse2_);
}

void Apu::clock_quarter_frame() { clock_envelopes(); }
void Apu::clock_half_frame()    { clock_length_counters(); clock_sweeps(); }

void Apu::clock_timers_cpu() {
    // Pulses tick every 2nd CPU cycle; we approximate by ticking on every CPU cycle with timer*2.
    auto tick_pulse = [](Pulse& p) {
        if (p.timer_counter == 0) {
            p.timer_counter = p.timer;
            p.seq_pos = (p.seq_pos + 1) & 7;
        } else --p.timer_counter;
    };
    // We're called once per CPU cycle; only tick pulses on alternating cycles using LSB of total_cycles_.
    if ((total_cycles_ & 1) == 0) {
        tick_pulse(pulse1_);
        tick_pulse(pulse2_);
    }
    // Triangle ticks every CPU cycle.
    if (triangle_.timer_counter == 0) {
        triangle_.timer_counter = triangle_.timer;
        if (triangle_.length && triangle_.linear_counter && triangle_.timer >= 2) {
            triangle_.seq_pos = (triangle_.seq_pos + 1) & 31;
        }
    } else --triangle_.timer_counter;
    // Noise ticks on every other CPU cycle (approximated like pulses).
    if ((total_cycles_ & 1) == 0) {
        if (noise_.timer_counter == 0) {
            noise_.timer_counter = noise_.timer_period;
            uint16_t fb = (noise_.shift & 1) ^ ((noise_.shift >> (noise_.mode ? 6 : 1)) & 1);
            noise_.shift >>= 1;
            noise_.shift |= fb << 14;
        } else --noise_.timer_counter;
    }
    // DMC ticks at its rate; one byte per 8 bits.
    if (dmc_.enabled) {
        if (dmc_.timer == 0) {
            dmc_.timer = dmc_.rate;
            if (!dmc_.silence) {
                if (dmc_.shift & 1) { if (dmc_.output_level <= 125) dmc_.output_level += 2; }
                else                { if (dmc_.output_level >= 2)   dmc_.output_level -= 2; }
            }
            dmc_.shift >>= 1;
            if (dmc_.bits_left) --dmc_.bits_left;
            if (dmc_.bits_left == 0) {
                dmc_.bits_left = 8;
                if (!dmc_.sample_buffer_filled) dmc_.silence = true;
                else {
                    dmc_.silence = false;
                    dmc_.shift = dmc_.sample_buffer;
                    dmc_.sample_buffer_filled = false;
                }
            }
        } else --dmc_.timer;

        if (!dmc_.sample_buffer_filled && dmc_.bytes_left) {
            if (dmc_reader_) dmc_.sample_buffer = dmc_reader_(dmc_.cur_addr);
            dmc_.sample_buffer_filled = true;
            dmc_.cur_addr = dmc_.cur_addr == 0xFFFF ? 0x8000 : dmc_.cur_addr + 1;
            if (--dmc_.bytes_left == 0) {
                if (dmc_.loop) {
                    dmc_.cur_addr = dmc_.sample_addr;
                    dmc_.bytes_left = dmc_.sample_len;
                } else if (dmc_.irq_enable) {
                    dmc_.irq_flag = true;
                }
            }
        }
    }
}

int16_t Apu::mix() const {
    float p1 = pulse1_.output();
    float p2 = pulse2_.output();
    float pulse_out = 0.0f;
    if (p1 + p2 > 0) pulse_out = 95.88f / (8128.0f / (p1 + p2) + 100.0f);
    float t = triangle_.output();
    float n = noise_.output();
    float d = dmc_.output_level;
    float tnd = 0.0f;
    if (t || n || d) {
        tnd = 159.79f / (1.0f / (t/8227.0f + n/12241.0f + d/22638.0f) + 100.0f);
    }
    float out = pulse_out + tnd; // 0..1
    int s = (int)((out - 0.5f) * 32000.0f);
    if (s > 32767) s = 32767;
    if (s < -32768) s = -32768;
    return (int16_t)s;
}

void Apu::emit_sample() {
    int16_t s = mix();
    sample_buffer_.push_back(s);
    sample_buffer_.push_back(s); // stereo duplicate
    if (sample_buffer_.size() >= 2048) {
        if (sample_cb_) sample_cb_(sample_buffer_);
        sample_buffer_.clear();
    }
}

void Apu::step(int cpu_cycles) {
    for (int i = 0; i < cpu_cycles; ++i) {
        clock_timers_cpu();
        ++total_cycles_;

        // Frame sequencer ~ 240Hz: divides CPU clock by 7457 (4-step) / 7457 (5-step).
        ++frame_div_;
        if (!frame_5step_) {
            switch (frame_div_) {
                case 7457:  clock_quarter_frame(); break;
                case 14913: clock_quarter_frame(); clock_half_frame(); break;
                case 22371: clock_quarter_frame(); break;
                case 29828: if (!frame_irq_inhibit_) frame_irq_ = true; break;
                case 29829: clock_quarter_frame(); clock_half_frame();
                            if (!frame_irq_inhibit_) frame_irq_ = true; break;
                case 29830: if (!frame_irq_inhibit_) frame_irq_ = true; frame_div_ = 0; break;
            }
        } else {
            switch (frame_div_) {
                case 7457:  clock_quarter_frame(); break;
                case 14913: clock_quarter_frame(); clock_half_frame(); break;
                case 22371: clock_quarter_frame(); break;
                case 37281: clock_quarter_frame(); clock_half_frame(); frame_div_ = 0; break;
            }
        }

        sample_accum_ += 1.0;
        if (sample_accum_ >= cycles_per_sample_) {
            sample_accum_ -= cycles_per_sample_;
            emit_sample();
        }
    }
}

void Apu::generate_samples(int n) {
    for (int i = 0; i < n; ++i) emit_sample();
}

int16_t Apu::get_channel_output(ApuChannel ch) const {
    switch (ch) {
        case ApuChannel::Pulse1:   return pulse1_.output();
        case ApuChannel::Pulse2:   return pulse2_.output();
        case ApuChannel::Triangle: return triangle_.output();
        case ApuChannel::Noise:    return noise_.output();
        case ApuChannel::Dmc:      return dmc_.output_level;
    }
    return 0;
}

} // namespace fcemu
