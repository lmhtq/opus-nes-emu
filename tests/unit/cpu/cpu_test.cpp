// cpu_test.cpp - CPU 单元测试 (stub)
#include "fcemu/cpu.h"
#include <cassert>
#include <cstdio>
#include <vector>

// Mock memory
uint8_t mock_memory[0x10000];

uint8_t mock_read(uint16_t addr) { return mock_memory[addr]; }
void mock_write(uint16_t addr, uint8_t val) { mock_memory[addr] = val; }

void test_reset() {
    printf("Test: CPU reset...\n");
    fcemu::Cpu6502 cpu;
    cpu.set_callbacks(mock_read, mock_write);
    cpu.reset();
    auto regs = cpu.registers();
    assert(regs.pc != 0);  // Should be from RESET vector
    assert(regs.sp == 0xFD);
    printf("  PASS\n");
}

void test_lda_immediate() {
    printf("Test: LDA immediate...\n");
    fcemu::Cpu6502 cpu;
    cpu.set_callbacks(mock_read, mock_write);
    cpu.reset();
    // Setup: LDA #$10 at $0200
    mock_memory[0x0200] = 0xA9;  // LDA immediate
    mock_memory[0x0201] = 0x10;
    mock_memory[0xFFFC] = 0x00;  // RESET vector low
    mock_memory[0xFFFD] = 0x02;  // RESET vector high
    cpu.reset();
    int cycles = cpu.step();  // Execute LDA #$10
    auto regs = cpu.registers();
    assert(regs.a == 0x10);
    assert(!cpu.get_flag(fcemu::Cpu6502::FLAG_Z));
    printf("  PASS (A=0x%02X, cycles=%d)\n", regs.a, cycles);
}

void test_lda_zero_page() {
    printf("Test: LDA zero page...\n");
    fcemu::Cpu6502 cpu;
    cpu.set_callbacks(mock_read, mock_write);
    mock_memory[0x0020] = 0x42;
    mock_memory[0x0200] = 0xA5;  // LDA zero page
    mock_memory[0x0201] = 0x20;
    mock_memory[0xFFFC] = 0x00;
    mock_memory[0xFFFD] = 0x02;
    cpu.reset();
    int cycles = cpu.step();
    auto regs = cpu.registers();
    assert(regs.a == 0x42);
    printf("  PASS (A=0x%02X)\n", regs.a);
}

void test_adc() {
    printf("Test: ADC...\n");
    fcemu::Cpu6502 cpu;
    cpu.set_callbacks(mock_read, mock_write);
    // LDA #$10, ADC #$20 => A = $30
    mock_memory[0x0200] = 0xA9; mock_memory[0x0201] = 0x10;
    mock_memory[0x0202] = 0x69; mock_memory[0x0203] = 0x20;
    mock_memory[0xFFFC] = 0x00; mock_memory[0xFFFD] = 0x02;
    cpu.reset();
    cpu.step();  // LDA #$10
    int cycles = cpu.step();  // ADC #$20
    auto regs = cpu.registers();
    assert(regs.a == 0x30);
    printf("  PASS (A=0x%02X, C=%d, Z=%d, N=%d)\n",
           regs.a, cpu.get_flag(fcemu::Cpu6502::FLAG_C),
           cpu.get_flag(fcemu::Cpu6502::FLAG_Z),
           cpu.get_flag(fcemu::Cpu6502::FLAG_N));
}

int main() {
    printf("=== fcemu CPU Tests ===\n\n");
    test_reset();
    test_lda_immediate();
    test_lda_zero_page();
    test_adc();
    printf("\nAll CPU tests passed!\n");
    return 0;
}
