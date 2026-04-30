// social_bridge.cpp - SocialBridge implementation.
#include "fcemu/social.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace fcemu {

namespace {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}
} // namespace

SocialBridge::SocialBridge() = default;
SocialBridge::~SocialBridge() { shutdown(); }

bool SocialBridge::init(const std::string& watch_path) {
    watch_path_ = watch_path;
    watch_mtime_ = 0;
    watch_offset_ = 0;
    return true;
}

void SocialBridge::shutdown() {
    std::lock_guard<std::mutex> g(queue_mu_);
    queue_.clear();
}

void SocialBridge::push_event(const SocialEvent& ev) {
    std::lock_guard<std::mutex> g(queue_mu_);
    queue_.push_back(ev);
}

SocialEvent SocialBridge::parse_line(const std::string& line) {
    SocialEvent ev;
    std::istringstream is(line);
    std::string cmd;
    if (!(is >> cmd)) return ev;
    cmd = lower(cmd);
    if      (cmd == "gift")  ev.type = SocialEventType::Gift;
    else if (cmd == "cheer") ev.type = SocialEventType::Cheer;
    else if (cmd == "chat")  ev.type = SocialEventType::Chat;
    else if (cmd == "vote")  ev.type = SocialEventType::Vote;
    else if (cmd == "shake") ev.type = SocialEventType::Shake;
    else return ev;

    switch (ev.type) {
        case SocialEventType::Gift: {
            is >> ev.kind;
            int c = 0; if (is >> c) ev.count = std::max(1, c);
            ev.duration_ms = 200 + 50 * ev.count;
            break;
        }
        case SocialEventType::Chat: {
            std::string rest; std::getline(is, rest);
            if (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
            ev.text = rest;
            break;
        }
        case SocialEventType::Vote: {
            is >> ev.text;          // up / down
            break;
        }
        case SocialEventType::Shake: {
            int i = 0; if (is >> i) ev.intensity = std::clamp(i, 1, 100);
            int d = 0; if (is >> d) ev.duration_ms = std::max(1, d);
            break;
        }
        case SocialEventType::Cheer: {
            ev.duration_ms = 400;
            break;
        }
        default: break;
    }
    return ev;
}

int SocialBridge::tick() {
    // 1) Watched file: read any new bytes appended since last poll.
    if (!watch_path_.empty()) {
        struct stat st{};
        if (::stat(watch_path_.c_str(), &st) == 0) {
            int64_t mt = (int64_t)st.st_mtime;
            if (mt != watch_mtime_ || (long)st.st_size < watch_offset_) {
                // File rotated/shrunk -> restart from current end.
                if ((long)st.st_size < watch_offset_) watch_offset_ = 0;
                watch_mtime_ = mt;
            }
            if ((long)st.st_size > watch_offset_) {
                std::ifstream f(watch_path_);
                if (f) {
                    f.seekg(watch_offset_);
                    std::string line;
                    while (std::getline(f, line)) {
                        if (line.empty()) continue;
                        auto ev = parse_line(line);
                        if (ev.type != SocialEventType::Unknown) push_event(ev);
                    }
                    watch_offset_ = (long)st.st_size;
                }
            }
        }
    }

    // 2) Drain queue.
    int n = 0;
    std::deque<SocialEvent> local;
    {
        std::lock_guard<std::mutex> g(queue_mu_);
        std::swap(local, queue_);
    }
    for (auto& ev : local) {
        if (handler_) handler_(ev);
        ++n; ++processed_;
    }
    return n;
}

} // namespace fcemu
