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
    void set_scene(const std::string& scene);
    VisualizationData get_visualization() const;

private:
    int sample_rate_;
    EqualizerBands eq_bands_;
    ReverbParams reverb_;
    StereoExpand stereo_;
    float bass_boost_ = 0.0f;
    float treble_boost_ = 0.0f;
    bool normalization_ = false;
    std::map<std::string, std::string> remix_tracks_;
    bool remix_enabled_ = false;
    std::string current_scene_;
    VisualizationData vis_data_;
    void apply_equalizer(std::vector<float>& samples);
    void apply_reverb(std::vector<float>& left, std::vector<float>& right);
    void apply_stereo_expand(std::vector<float>& left, std::vector<float>& right);
};

} // namespace fcemu
