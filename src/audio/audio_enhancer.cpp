// audio_enhancer.cpp - Stereo widening, bass/treble shelf, simple peak normalization.
#include "fcemu/audio_enhancer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace fcemu {

AudioEnhancer::AudioEnhancer() : sample_rate_(44100) {
    std::memset(eq_bands_.band, 0, sizeof(eq_bands_.band));
}

bool AudioEnhancer::init(int sample_rate) { sample_rate_ = sample_rate; return true; }
void AudioEnhancer::shutdown() {}

static inline float db_to_lin(float db) { return std::pow(10.0f, db / 20.0f); }

void AudioEnhancer::process_samples(const std::vector<int16_t>& in, std::vector<int16_t>& out) {
    out = in;
    if (out.size() < 2) return;

    float bass_g   = db_to_lin(bass_boost_);
    float treble_g = db_to_lin(treble_boost_);
    float prev_l = 0, prev_r = 0;

    bool stereo = out.size() % 2 == 0;
    for (size_t i = 0; i + 1 < out.size(); i += 2) {
        float l = out[i] / 32768.0f;
        float r = out[i + 1] / 32768.0f;

        // 1-pole shelves around lp/hp cutoff.
        float lp_l = prev_l * 0.7f + l * 0.3f;
        float lp_r = prev_r * 0.7f + r * 0.3f;
        prev_l = lp_l; prev_r = lp_r;
        float hp_l = l - lp_l;
        float hp_r = r - lp_r;
        l = lp_l * bass_g + hp_l * treble_g + (l - lp_l - hp_l);
        r = lp_r * bass_g + hp_r * treble_g + (r - lp_r - hp_r);

        // Stereo expand via mid/side.
        if (stereo && stereo_.enabled) {
            float m = 0.5f * (l + r);
            float s = 0.5f * (l - r);
            s *= (1.0f + stereo_.width);
            l = m + s;
            r = m - s;
        }

        if (normalization_) {
            l = std::tanh(l * 1.2f);
            r = std::tanh(r * 1.2f);
        }

        l = std::clamp(l, -1.0f, 1.0f);
        r = std::clamp(r, -1.0f, 1.0f);
        out[i]     = (int16_t)(l * 32767.0f);
        out[i + 1] = (int16_t)(r * 32767.0f);
    }

    // Light visualization: down-sampled waveform.
    vis_data_.waveform.clear();
    vis_data_.waveform.reserve(64);
    size_t step = std::max<size_t>(1, out.size() / 64);
    for (size_t i = 0; i < out.size(); i += step) {
        vis_data_.waveform.push_back(out[i] / 32768.0f);
    }
}

void AudioEnhancer::set_equalizer(const EqualizerBands& b)   { eq_bands_ = b; }
void AudioEnhancer::set_reverb(const ReverbParams& p)        { reverb_ = p; }
void AudioEnhancer::set_stereo_expand(const StereoExpand& e) { stereo_ = e; }
void AudioEnhancer::set_bass_boost(float db)                 { bass_boost_ = db; }
void AudioEnhancer::set_treble_boost(float db)               { treble_boost_ = db; }
void AudioEnhancer::set_normalization(bool e, float)         { normalization_ = e; }
bool AudioEnhancer::load_remix_track(const std::string& n, const std::string& p) {
    remix_tracks_[n] = p; return true;
}
void AudioEnhancer::enable_remix(bool e) { remix_enabled_ = e; }
void AudioEnhancer::set_remix_volume(float)        {}
void AudioEnhancer::set_scene(const std::string& s){ current_scene_ = s; }
VisualizationData AudioEnhancer::get_visualization() const { return vis_data_; }

void AudioEnhancer::apply_equalizer(std::vector<float>&) {}
void AudioEnhancer::apply_reverb(std::vector<float>&, std::vector<float>&) {}
void AudioEnhancer::apply_stereo_expand(std::vector<float>&, std::vector<float>&) {}

} // namespace fcemu
