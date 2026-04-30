// apu_test.cpp - APU 单元测试 (stub)
#include "fcemu/apu.h"
#include <cassert>
#include <cstdio>
#include <vector>

void test_init() {
    printf("Test: APU init...\n");
    fcemu::Ap u apu;
    assert(apu.init(44100));
    printf("  PASS (sample rate 44100)\n");
}

void test_channel_output() {
    printf("Test: Channel output...\n");
    fcemu::Ap u apu;
    apu.init(44100);
    apu.reset();
    // Check that channels are initially silent
    assert(apu.get_channel_output(fcemu::Ap uChannel::Pulse1) == 0);
    assert(apu.get_channel_output(fcemu::Ap uChannel::Pulse2) == 0);
    printf("  PASS (all channels silent after reset)\n");
}

int main() {
    printf("=== fcemu APU Tests ===\n\n");
    test_init();
    test_channel_output();
    printf("\nAll APU tests passed!\n");
    return 0;
}
