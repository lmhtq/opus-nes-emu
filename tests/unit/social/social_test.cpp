// social_test.cpp - SocialBridge parse + dispatch.
#include "fcemu/social.h"
#include <cassert>
#include <cstdio>
#include <fstream>

int main() {
    using namespace fcemu;
    std::printf("=== fcemu Social Bridge Tests ===\n");

    auto e1 = SocialBridge::parse_line("gift bullet 5");
    assert(e1.type == SocialEventType::Gift);
    assert(e1.kind == "bullet");
    assert(e1.count == 5);

    auto e2 = SocialBridge::parse_line("cheer");
    assert(e2.type == SocialEventType::Cheer);

    auto e3 = SocialBridge::parse_line("chat hello world");
    assert(e3.type == SocialEventType::Chat);
    assert(e3.text == "hello world");

    auto e4 = SocialBridge::parse_line("vote up");
    assert(e4.type == SocialEventType::Vote);
    assert(e4.text == "up");

    auto e5 = SocialBridge::parse_line("shake 70 250");
    assert(e5.type == SocialEventType::Shake);
    assert(e5.intensity == 70);
    assert(e5.duration_ms == 250);

    auto e6 = SocialBridge::parse_line("nope blah");
    assert(e6.type == SocialEventType::Unknown);

    // Dispatch.
    SocialBridge sb;
    sb.init();
    int gifts = 0, cheers = 0;
    sb.set_handler([&](const SocialEvent& ev){
        if (ev.type == SocialEventType::Gift)  ++gifts;
        if (ev.type == SocialEventType::Cheer) ++cheers;
    });
    sb.push_event(e1);
    sb.push_event(e2);
    sb.push_event(e2);
    int n = sb.tick();
    assert(n == 3);
    assert(gifts == 1 && cheers == 2);

    // File watcher: write some lines to a temp file and verify they get
    // dispatched on the next tick.
    const char* tmp = "/tmp/fcemu_social_test_events.txt";
    {
        std::ofstream(tmp).close(); // truncate
    }
    SocialBridge fw;
    fw.init(tmp);
    int picked = 0;
    fw.set_handler([&](const SocialEvent&){ ++picked; });
    fw.tick(); // baseline (file empty)
    {
        std::ofstream f(tmp);
        f << "gift heart 1\n";
        f << "cheer\n";
        f << "shake 50\n";
    }
    int n2 = fw.tick();
    assert(n2 >= 3);
    assert(picked >= 3);

    std::printf("PASS\n");
    return 0;
}
