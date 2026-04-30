// include/fcemu/audio_enhancer.h
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>

namespace fcemu {

struct EqualizerBands {
    float band[10];
};

struct ReverbParams {
    float room_size = 0.5f;
    float damping = 0.5f;
    float wet_mix = 0.3f;
    float dry_mix = 0.7f;
    bool enabled = false;
};

struct StereoExpand {
    float width = 0.5f;
    bool enabled = false;
};

struct VisualizationData {
    std::vector<float> waveform;
    std::vector<float> spectrum;
};

class AudioEnhancer {
public:
    AudioEnhancer();
    bool init(int sample_rate = 44100);
    void shutdown();
    void process_samples(const std::vector<int16_t>& input,
                       std::vector<int16_t>& output);
    void set_equalizer(const EqualizerBands& bands);
    void set_reverb(const ReverbParams& params);
    void set_stereo_expand(const StereoExpand& expand);
    void set_bass_boost(float db);
    void set_treble_boost(float db);
    void set_normalization(bool enable, float target_db = -3.0f);
    bool load_remix_track(const std::string& track_name, const std::string& file_path);
    void enable_remix(bool enable);
    void set_remix_volume(float vol);
    // Trigger a one-shot mix-in of a previously loaded raw S16-LE 44.1kHz
    // stereo track. Mixes during subsequent process_samples() calls until
    // the source is exhausted.
    void trigger_remix_oneshot(const std::string& track_name);

    // Apply a named scene preset to EQ/reverb/stereo params (e.g. "action",
    // "calm", "boss", "menu", "victory"). Unknown names are no-ops.
    void set_scene(const std::string& scene);
    const std::string& current_scene() const { return current_scene_; }
    VisualizationData get_visualization() const;

private:
    int sample_rate_;
    EqualizerBands eq_bands_;
    ReverbParams reverb_;
    StereoExpand stereo_;
    float bass_boost_ = 0.0f;
    float treble_boost_ = 0.0f;
    bool normalization_ = false;
    std::map<std::string, std::string> remix_tracks_;          // name -> path
    std::map<std::string, std::vector<int16_t>> remix_pcm_;    // name -> raw S16
    std::vector<int16_t>                       active_remix_;  // current one-shot
    size_t                                     remix_pos_ = 0;
    float                                      remix_vol_ = 0.7f;
    bool remix_enabled_ = false;
    std::string current_scene_;
    VisualizationData vis_data_;
    void apply_equalizer(std::vector<float>& samples);
    void apply_reverb(std::vector<float>& left, std::vector<float>& right);
    void apply_stereo_expand(std::vector<float>& left, std::vector<float>& right);
};

} // namespace fcemu
