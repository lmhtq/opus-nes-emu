// replay.cpp - Ring-buffer based replay manager.
#include "fcemu/replay.h"
#include <algorithm>
#include <cstdio>

namespace fcemu {

ReplayBuffer::ReplayBuffer() : max_duration_ms_(60000), write_pos_(0), wrapped_(false) {}

void ReplayBuffer::init(int max_duration_seconds) {
    max_duration_ms_ = max_duration_seconds * 1000;
    size_t cap = (size_t)((max_duration_ms_ / 16) + 1); // ~60Hz
    buffer_.assign(cap, FrameData{});
    write_pos_ = 0; wrapped_ = false;
}

void ReplayBuffer::shutdown() { buffer_.clear(); write_pos_ = 0; wrapped_ = false; }

void ReplayBuffer::push_frame(const FrameData& f) {
    if (buffer_.empty()) return;
    buffer_[write_pos_] = f;
    write_pos_ = (write_pos_ + 1) % buffer_.size();
    if (write_pos_ == 0) wrapped_ = true;
}

std::vector<FrameData> ReplayBuffer::get_last_n_seconds(int seconds) const {
    if (buffer_.empty()) return {};
    size_t n = std::min<size_t>((size_t)(seconds * 60), wrapped_ ? buffer_.size() : write_pos_);
    std::vector<FrameData> out; out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        size_t idx = (write_pos_ + buffer_.size() - n + i) % buffer_.size();
        out.push_back(buffer_[idx]);
    }
    return out;
}

std::vector<FrameData> ReplayBuffer::get_highlight_clip(const Highlight& hl) const {
    return get_last_n_seconds(hl.duration_ms / 1000);
}

int ReplayBuffer::current_duration_ms() const {
    return (int)((wrapped_ ? buffer_.size() : write_pos_) * 16);
}

bool ReplayBuffer::is_full() const { return wrapped_; }

ReplayManager::ReplayManager() = default;

bool ReplayManager::init() { buffer_.init(60); return true; }
void ReplayManager::shutdown() { buffer_.shutdown(); highlights_.clear(); }
void ReplayManager::start_recording() { recording_ = true; }
void ReplayManager::stop_recording()  { recording_ = false; }

void ReplayManager::check_and_save_highlights() {
    // Heuristic placeholder — caller code drives via mark_highlight().
}

void ReplayManager::mark_highlight(HighlightType type, const std::string& desc) {
    Highlight h{}; h.type = type; h.description = desc;
    h.timestamp = (uint64_t)buffer_.current_duration_ms();
    highlights_.push_back(h);
}

bool ReplayManager::generate_clip(const Highlight& hl, const std::string& output) {
    // Without a video encoder dep, dump first frame as raw PPM.
    auto frames = buffer_.get_highlight_clip(hl);
    if (frames.empty()) return false;
    std::FILE* f = std::fopen(output.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n256 240\n255\n");
    auto& v = frames.back().video;
    for (int i = 0; i < 256 * 240; ++i) {
        if ((size_t)(i * 4 + 2) >= v.size()) break;
        std::fputc(v[i*4+0], f);
        std::fputc(v[i*4+1], f);
        std::fputc(v[i*4+2], f);
    }
    std::fclose(f);
    return true;
}

bool ReplayManager::generate_last_n_seconds(int seconds, const std::string& output) {
    Highlight hl{}; hl.type = HighlightType::Custom; hl.duration_ms = seconds * 1000;
    return generate_clip(hl, output);
}

} // namespace fcemu
