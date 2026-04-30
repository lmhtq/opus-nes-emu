// audio_enhancer.cpp - Stereo widening, EQ shelves, scenes, remix mix-in.
#include "fcemu/audio_enhancer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

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

        // Mix in active remix one-shot (stereo, advancing remix_pos_).
        if (remix_enabled_ && remix_pos_ + 1 < active_remix_.size()) {
            float rl = active_remix_[remix_pos_]   / 32768.0f;
            float rr = active_remix_[remix_pos_+1] / 32768.0f;
            l = l * (1.0f - 0.5f * remix_vol_) + rl * remix_vol_;
            r = r * (1.0f - 0.5f * remix_vol_) + rr * remix_vol_;
            remix_pos_ += 2;
        }

        l = std::clamp(l, -1.0f, 1.0f);
        r = std::clamp(r, -1.0f, 1.0f);
        out[i]     = (int16_t)(l * 32767.0f);
        out[i + 1] = (int16_t)(r * 32767.0f);
    }
    if (remix_pos_ >= active_remix_.size()) {
        active_remix_.clear();
        remix_pos_ = 0;
    }

    // Light visualization: down-sampled waveform + naive spectrum bins.
    vis_data_.waveform.clear();
    vis_data_.waveform.reserve(64);
    size_t step = std::max<size_t>(1, out.size() / 64);
    for (size_t i = 0; i < out.size(); i += step) {
        vis_data_.waveform.push_back(out[i] / 32768.0f);
    }
    vis_data_.spectrum.assign(8, 0.0f);
    for (size_t i = 0; i < out.size(); ++i) {
        vis_data_.spectrum[i & 7] += std::abs(out[i] / 32768.0f);
    }
    for (auto& v : vis_data_.spectrum)
        v /= std::max<size_t>(1, out.size() / 8);
}

void AudioEnhancer::set_equalizer(const EqualizerBands& b)   { eq_bands_ = b; }
void AudioEnhancer::set_reverb(const ReverbParams& p)        { reverb_ = p; }
void AudioEnhancer::set_stereo_expand(const StereoExpand& e) { stereo_ = e; }
void AudioEnhancer::set_bass_boost(float db)                 { bass_boost_ = db; }
void AudioEnhancer::set_treble_boost(float db)               { treble_boost_ = db; }
void AudioEnhancer::set_normalization(bool e, float)         { normalization_ = e; }

bool AudioEnhancer::load_remix_track(const std::string& n, const std::string& p) {
    remix_tracks_[n] = p;
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return false;
    size_t sz = (size_t)f.tellg();
    f.seekg(0);
    std::vector<int16_t> pcm(sz / 2);
    f.read(reinterpret_cast<char*>(pcm.data()), pcm.size() * 2);
    remix_pcm_[n] = std::move(pcm);
    return true;
}
void AudioEnhancer::enable_remix(bool e)         { remix_enabled_ = e; }
void AudioEnhancer::set_remix_volume(float v)    { remix_vol_ = std::clamp(v, 0.0f, 1.0f); }

void AudioEnhancer::trigger_remix_oneshot(const std::string& n) {
    auto it = remix_pcm_.find(n);
    if (it == remix_pcm_.end()) return;
    active_remix_ = it->second;
    remix_pos_ = 0;
}

void AudioEnhancer::set_scene(const std::string& s) {
    current_scene_ = s;
    // Tweak EQ + stereo + reverb based on coarse scene category.
    EqualizerBands bands{}; std::memset(bands.band, 0, sizeof(bands.band));
    StereoExpand st{};   ReverbParams rv{};
    if (s == "action" || s == "boss") {
        bass_boost_   = 4.0f;
        treble_boost_ = 2.0f;
        st.enabled = true; st.width = 0.7f;
        rv.enabled = false;
    } else if (s == "calm" || s == "menu") {
        bass_boost_   = 0.0f;
        treble_boost_ = 0.0f;
        st.enabled = true; st.width = 0.3f;
        rv.enabled = true; rv.wet_mix = 0.15f; rv.dry_mix = 0.85f;
    } else if (s == "victory" || s == "item") {
        bass_boost_   = 2.0f;
        treble_boost_ = 4.0f;
        st.enabled = true; st.width = 0.5f;
    } else {
        // default
        bass_boost_ = treble_boost_ = 0.0f;
        st.enabled = false;
    }
    set_stereo_expand(st);
    set_reverb(rv);
    set_equalizer(bands);
}

VisualizationData AudioEnhancer::get_visualization() const { return vis_data_; }

void AudioEnhancer::apply_equalizer(std::vector<float>&) {}
void AudioEnhancer::apply_reverb(std::vector<float>&, std::vector<float>&) {}
void AudioEnhancer::apply_stereo_expand(std::vector<float>&, std::vector<float>&) {}

} // namespace fcemu
