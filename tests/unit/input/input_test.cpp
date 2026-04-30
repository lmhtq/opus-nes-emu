// input_test.cpp - Standard controller LSB-first shift protocol.
#include "fcemu/input.h"
#include <cassert>
#include <cstdio>

int main() {
    printf("=== fcemu Input Tests ===\n");
    fcemu::StandardController c;
    c.set_button(fcemu::Button::A, true);
    c.set_button(fcemu::Button::Right, true);
    c.strobe();
    // First read = A (bit 0), then B(0), Select(0), Start(0), Up(0), Down(0), Left(0), Right(1)
    int seq[8] = {1, 0, 0, 0, 0, 0, 0, 1};
    for (int i = 0; i < 8; ++i) {
        uint8_t v = c.read();
        assert(v == seq[i]);
    }
    // After 8 reads, must return 1.
    assert(c.read() == 1);

    // ---- Turbo (rapid-fire) test ----------------------------------------
    fcemu::StandardController t;
    t.set_button(fcemu::Button::A, true);
    t.set_turbo(fcemu::Button::A, true);
    t.set_turbo_rate(1); // toggle every frame
    auto read_a = [&]{ t.strobe(); return (int)(t.read() & 1); };

    // Phase starts low → A appears released even though held.
    int v0 = read_a();
    t.tick_turbo();
    int v1 = read_a();
    t.tick_turbo();
    int v2 = read_a();
    // Must oscillate between 0 and 1 across frames.
    assert(v0 != v1);
    assert(v1 != v2);
    assert(v0 == v2);

    // Disabling turbo → A is steady high.
    t.set_turbo(fcemu::Button::A, false);
    for (int i = 0; i < 4; ++i) { assert(read_a() == 1); t.tick_turbo(); }

    printf("PASS\n");
    return 0;
}
