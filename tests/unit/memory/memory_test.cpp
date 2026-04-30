// memory_test.cpp - Memory 单元测试 (stub)
#include "fcemu/memory.h"
#include <cassert>
#include <cstdio>

uint8_t mock_read(uint16_t addr) { return 0; }
void mock_write(uint16_t addr, uint8_t val) {}

void test_ram_mirror() {
    printf("Test: RAM mirror...\n");
    fcemu::Memory mem;
    mem.reset();
    mem.write(0x0000, 0x42);
    assert(mem.read(0x0000) == 0x42);
    assert(mem.read(0x0800) == 0x42);  // Mirror
    assert(mem.read(0x1000) == 0x42);  // Mirror
    printf("  PASS (RAM mirror works)\n");
}

int main() {
    printf("=== fcemu Memory Tests ===\n\n");
    test_ram_mirror();
    printf("\nAll Memory tests passed!\n");
    return 0;
}
