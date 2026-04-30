// include/fcemu/social.h - Live interaction bridge (chat / gifts / votes).
//
// In-process API + optional file watcher. Parses newline-delimited lines:
//
//   gift <kind> [count]    e.g. "gift bullet 5"     -> burst-press P1 A
//   cheer                                            -> hit_flash + rumble
//   chat <text>            (logged only)
//   vote <up|down>         (logged only)
//   shake [intensity_ms]                             -> trigger_shake
//
// Effects are applied via callbacks set by the owner. This avoids hard
// dependencies on Haptics / Video / Input from the social module.
#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace fcemu {

enum class SocialEventType { Gift, Cheer, Chat, Vote, Shake, Unknown };

struct SocialEvent {
    SocialEventType type = SocialEventType::Unknown;
    std::string kind;     // "bullet", "heart", "fire", ...
    std::string text;     // chat text / vote dir
    int  count = 1;
    int  intensity = 100; // 0-100 generic
    int  duration_ms = 250;
};

using SocialHandler = std::function<void(const SocialEvent&)>;

class SocialBridge {
public:
    SocialBridge();
    ~SocialBridge();

    bool init(const std::string& watch_path = "");
    void shutdown();

    // Callback fires in tick() for each pending event.
    void set_handler(SocialHandler h) { handler_ = std::move(h); }

    // Push an event from any thread (test code, future network plug-in).
    void push_event(const SocialEvent& ev);

    // Drain queue + check the watch file (if any) for new lines. Call once
    // per frame from main loop. Returns number of events dispatched.
    int  tick();

    // Parse a single text line into an event. Public for testing.
    static SocialEvent parse_line(const std::string& line);

    int events_processed() const { return processed_; }

private:
    SocialHandler handler_;
    std::deque<SocialEvent> queue_;
    std::mutex queue_mu_;

    std::string watch_path_;
    int64_t     watch_mtime_ = 0;
    long        watch_offset_ = 0;
    int         processed_ = 0;
};

} // namespace fcemu
