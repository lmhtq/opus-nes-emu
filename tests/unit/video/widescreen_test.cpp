// widescreen_test.cpp - VideoEnhancer widescreen 320x240 expansion.
#include "fcemu/video_enhancer.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

int main() {
    using namespace fcemu;
    std::printf("=== fcemu Widescreen Tests ===\n");

    std::vector<uint8_t> in(256 * 240 * 4, 0);
    // Fill column 0 red, column 255 blue; center solid green.
    for (int y = 0; y < 240; ++y) {
        uint8_t* row = in.data() + y * 256 * 4;
        row[0] = 255; row[1] = 0; row[2] = 0; row[3] = 255;
        for (int x = 1; x < 255; ++x) {
            row[x*4+0] = 0; row[x*4+1] = 200; row[x*4+2] = 0; row[x*4+3] = 255;
        }
        row[255*4+0] = 0; row[255*4+1] = 0; row[255*4+2] = 255; row[255*4+3] = 255;
    }

    VideoEnhancer v; v.init();
    v.set_passthrough(true);

    // Default (256x240, no widescreen).
    int w = 0, h = 0;
    auto* p1 = v.process(in.data(), &w, &h);
    assert(p1 != nullptr); assert(w == 256 && h == 240);

    // Enable widescreen: should produce 320x240 and replicate edge cols.
    v.enable_widescreen(true);
    auto* p2 = v.process(in.data(), &w, &h);
    assert(p2 != nullptr); assert(w == 320 && h == 240);

    // Center pixel (160, 120) should be green; left band (5, 120) red-tinted.
    int center_g = p2[(120*320 + 160)*4 + 1];
    int left_r   = p2[(120*320 + 5)*4 + 0];
    int right_b  = p2[(120*320 + 315)*4 + 2];
    assert(center_g > 100);
    assert(left_r  > 50);
    assert(right_b > 50);

    std::printf("PASS\n");
    return 0;
}
