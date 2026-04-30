// Unit tests for fcemu Overlay primitives + toast queue.
#include "fcemu/overlay.h"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace fcemu;

static void test_put_and_blend() {
    std::vector<uint8_t> buf(8 * 8 * 4, 0);
    Overlay::put_pixel(buf.data(), 8, 8, 3, 4, RGBA{10, 20, 30, 255});
    uint8_t* p = buf.data() + (4 * 8 + 3) * 4;
    assert(p[0] == 10 && p[1] == 20 && p[2] == 30 && p[3] == 255);

    // Out-of-bounds writes must not crash or affect buffer.
    Overlay::put_pixel(buf.data(), 8, 8, -1, 0, RGBA{1, 2, 3, 4});
    Overlay::put_pixel(buf.data(), 8, 8, 0, 99, RGBA{1, 2, 3, 4});

    // 50% alpha blend onto white background.
    std::vector<uint8_t> bg(4, 255);
    Overlay::blend_pixel(bg.data(), 1, 1, 0, 0, RGBA{0, 0, 0, 128});
    assert(bg[0] < 200 && bg[0] > 100);  // ~127
}

static void test_fill_and_text() {
    std::vector<uint8_t> buf(64 * 16 * 4, 0);
    Overlay::fill_rect(buf.data(), 64, 16, 4, 4, 8, 8, Overlay::White);
    // Center of the rect should now be white.
    uint8_t* p = buf.data() + (8 * 64 + 8) * 4;
    assert(p[0] == 255 && p[1] == 255 && p[2] == 255);

    // Drawing visible text must light up at least some pixels.
    std::vector<uint8_t> tbuf(64 * 16 * 4, 0);
    Overlay::draw_text(tbuf.data(), 64, 16, 0, 0, "AB", Overlay::White);
    int lit = 0;
    for (size_t i = 0; i < tbuf.size(); i += 4)
        if (tbuf[i] > 0) ++lit;
    assert(lit > 5);
    assert(Overlay::text_width("hello") == 40);
}

static void test_toasts() {
    Overlay ov;
    ov.post_toast("hello", 1.0f);
    ov.post_toast("world", 0.5f);
    std::vector<uint8_t> buf(64 * 32 * 4, 0);
    ov.render_toasts(buf.data(), 64, 32);

    ov.update(0.6f);  // "world" expires
    int before = 0;
    for (size_t i = 0; i < buf.size(); i += 4) if (buf[i] > 0) ++before;
    assert(before > 0);

    ov.update(2.0f);  // "hello" also expires
    std::vector<uint8_t> empty(64 * 32 * 4, 0);
    ov.render_toasts(empty.data(), 64, 32);
    int after = 0;
    for (size_t i = 0; i < empty.size(); i += 4) if (empty[i] > 0) ++after;
    assert(after == 0);
}

int main() {
    test_put_and_blend();
    test_fill_and_text();
    test_toasts();
    std::printf("[overlay] OK\n");
    return 0;
}
