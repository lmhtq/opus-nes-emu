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
    if (buffer_.size() < static_cast<size_t>(max_duration_ms_ / 16)) {  // ~16ms per frame
        buffer_.push_back(frame);
        write_pos_ = buffer_.size() - 1;
    } else {
        buffer_[write_pos_] = frame;
        write_pos_ = (write_pos_ + 1) % buffer_.size();
        if (write_pos_ == 0) wrapped_ = true;
    }
}

std::vector<FrameData> ReplayBuffer::get_highlight_clip(const Highlight& hl) const {
    // TODO: Extract frames around highlight timestamp
    return {};
}

std::vector<FrameData> ReplayBuffer::get_last_n_seconds(int seconds) const {
    // TODO: Get last N seconds of frames
    return {};
}

int ReplayBuffer::current_duration_ms() const {
    return static_cast<int>(buffer_.size()) * 16;  // ~16ms per frame
}

int ReplayBuffer::max_duration_ms() const { return max_duration_ms_; }
bool ReplayBuffer::is_full() const { return wrapped_; }

// --- HighlightDetector ---

HighlightDetector::HighlightDetector() : cpu_(nullptr), ppu_(nullptr), apu_(nullptr) {}

void HighlightDetector::set_callbacks(Cpu6502* cpu, Ppu* ppu, Apu* apu) {
    cpu_ = cpu;
    ppu_ = ppu;
    apu_ = apu;
}

bool HighlightDetector::check_highlight(HighlightType type, Highlight& out) const {
    // TODO: Implement detection logic
    return false;
}

void HighlightDetector::register_detector(HighlightType type, DetectCallback cb) {
    // TODO
}

bool HighlightDetector::detect_1up() const { return false; }  // TODO
bool HighlightDetector::detect_boss_kill() const { return false; }  // TODO
bool HighlightDetector::detect_hidden_item() const { return false; }  // TODO
bool HighlightDetector::detect_level_clear() const { return false; }  // TODO

// --- VideoGenerator ---

VideoGenerator::VideoGenerator() : width_(0), height_(0), fps_(0), filter_(Filter::None) {}

bool VideoGenerator::init(int width, int height, int fps) {
    width_ = width; height_ = height; fps_ = fps;
    printf("VideoGenerator: Init %dx%d @ %d fps\n", width, height, fps);
    return true;
}

bool VideoGenerator::generate_mp4(const std::vector<FrameData>& frames,
                                  const std::string& output_path) {
    printf("VideoGenerator: Generate MP4 to %s (%zu frames)\n",
           output_path.c_str(), frames.size());
    // TODO: Use ffmpeg or similar
    return false;
}

void VideoGenerator::set_filter(Filter f) { filter_ = f; }

// --- ReplayManager ---

ReplayManager::ReplayManager()
    : recording_(false), auto_detect_(true) {}

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
bool ReplayManager::recording() const { return recording_; }

void ReplayManager::check_and_save_highlights() {
    // TODO
}

const std::vector<Highlight>& ReplayManager::highlights() const { return highlights_; }

void ReplayManager::mark_highlight(HighlightType type, const std::string& desc) {
    printf("ReplayManager: Manual highlight mark: type=%d desc=%s\n",
           static_cast<int>(type), desc.c_str());
    Highlight hl;
    hl.type = type;
    hl.description = desc;
    highlights_.push_back(hl);
}

bool ReplayManager::generate_clip(const Highlight& hl, const std::string& output) {
    return generator_.generate_mp4(buffer_.get_highlight_clip(hl), output);
}

bool ReplayManager::generate_last_n_seconds(int seconds, const std::string& output) {
    return generator_.generate_mp4(buffer_.get_last_n_seconds(seconds), output);
}

bool ReplayManager::share_to_platform(const std::string& clip_path,
                                        const std::string& platform) {
    printf("ReplayManager: Share %s to %s\n", clip_path.c_str(), platform.c_str());
    return false;  // TODO
}

void ReplayManager::set_buffer_duration(int seconds) { buffer_.init(seconds); }
void ReplayManager::set_auto_detect(bool enable) { auto_detect_ = enable; }

}  // namespace fcemu
