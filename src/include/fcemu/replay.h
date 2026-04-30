// include/fcemu/replay.h
#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace fcemu {

struct FrameData {
    std::vector<uint8_t> video;
    std::vector<int16_t> audio;
    uint64_t timestamp;
};

enum class HighlightType { OneUp, LevelClear, BossKill, HiddenItem, Custom };

struct Highlight {
    HighlightType type;
    uint64_t timestamp;
    std::string description;
    int duration_ms = 5000;
};

class ReplayBuffer {
public:
    ReplayBuffer();
    void init(int max_duration_seconds = 60);
    void shutdown();
    void push_frame(const FrameData& frame);
    std::vector<FrameData> get_highlight_clip(const Highlight& hl) const;
    std::vector<FrameData> get_last_n_seconds(int seconds) const;
    int current_duration_ms() const;
    int max_duration_ms() const { return max_duration_ms_; }
    bool is_full() const;

private:
    std::vector<FrameData> buffer_;
    int max_duration_ms_;
    size_t write_pos_ = 0;
    bool wrapped_ = false;
};

class ReplayManager {
public:
    ReplayManager();
    bool init();
    void shutdown();
    void start_recording();
    void stop_recording();
    bool recording() const { return recording_; }
    void check_and_save_highlights();
    const std::vector<Highlight>& highlights() const { return highlights_; }
    void mark_highlight(HighlightType type, const std::string& desc);
    bool generate_clip(const Highlight& hl, const std::string& output);
    bool generate_last_n_seconds(int seconds, const std::string& output);

private:
    ReplayBuffer buffer_;
    std::vector<Highlight> highlights_;
    bool recording_ = false;
};

} // namespace fcemu
