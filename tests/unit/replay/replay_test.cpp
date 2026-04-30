// replay_test.cpp - Ring buffer wrap & last-N-seconds extraction.
#include "fcemu/replay.h"
#include <cassert>
#include <cstdio>

int main() {
    printf("=== fcemu Replay Tests ===\n");
    fcemu::ReplayBuffer rb;
    rb.init(1); // 1 sec ~= 60 frames
    fcemu::FrameData f;
    for (int i = 0; i < 200; ++i) {
        f.timestamp = (uint64_t)i;
        rb.push_frame(f);
    }
    assert(rb.is_full());
    auto last = rb.get_last_n_seconds(1);
    // Last frame should be the most recent (timestamp 199).
    assert(!last.empty());
    assert(last.back().timestamp == 199);
    printf("PASS\n");
    return 0;
}
