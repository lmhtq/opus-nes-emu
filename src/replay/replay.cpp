// replay.cpp - Replay manager (stub)
#include "fcemu/replay.h"
#include <cstdio>

namespace fcemu {

// --- ReplayBuffer ---

ReplayBuffer::ReplayBuffer() : max_duration_ms_(60000), write_pos_(0), wrapped_(false) {}

void ReplayBuffer::init(int max_duration_seconds) {
    max_duration_ms_ = max_duration_seconds * 1000;
    buffer_.clear();
    write_pos_ = 0;
    wrapped_ = false;
}

void ReplayBuffer::shutdown() {
    buffer_.clear();
}

void ReplayBuffer::push_frame(const FrameData& frame) {
    if (buffer_.size() < static_cast<size_t>(max_duration_ms_ / 16)) {
        buffer_.push_back(frame);
        write_pos_ = buffer_.size() - 1;
    } else {
        buffer_[write_pos_] = frame;
        write_pos_ = (write_pos_ + 1) % buffer_.size();
        if (write_pos_ == 0) wrapped_ = true;
    }
}

std::vector<FrameData> ReplayBuffer::get_highlight_clip(const Highlight& hl) const {
    return {};
}

std::vector<FrameData> ReplayBuffer::get_last_n_seconds(int seconds) const {
    return {};
}

int ReplayBuffer::current_duration_ms() const {
    return static_cast<int>(buffer_.size()) * 16;
}

bool ReplayBuffer::is_full() const { return wrapped_; }

// --- ReplayManager ---

ReplayManager::ReplayManager() : recording_(false) {}

bool ReplayManager::init() {
    printf("ReplayManager: Initializing...\n");
    buffer_.init(60);
    return true;
}

void ReplayManager::shutdown() {
    buffer_.shutdown();
    highlights_.clear();
}

void ReplayManager::start_recording() { recording_ = true; }
void ReplayManager::stop_recording() { recording_ = false; }

void ReplayManager::check_and_save_highlights() {
    // TODO
}

void ReplayManager::mark_highlight(HighlightType type, const std::string& desc) {
    printf("ReplayManager: Manual highlight mark: type=%d desc=%s\n",
           static_cast<int>(type), desc.c_str());
    Highlight hl;
    hl.type = type;
    hl.description = desc;
    highlights_.push_back(hl);
}

bool ReplayManager::generate_clip(const Highlight& hl, const std::string& output) {
    // TODO: Generate video clip
    return false;
}

bool ReplayManager::generate_last_n_seconds(int seconds, const std::string& output) {
    // TODO: Generate video
    return false;
}

}  // namespace fcemu
