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
    printf("PASS\n");
    return 0;
}
