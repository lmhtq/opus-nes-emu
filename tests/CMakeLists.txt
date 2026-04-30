# tests/CMakeLists.txt
enable_testing()

# 查找 Google Test
find_package(GTest REQUIRED)

# CPU 测试
add_executable(cpu_test
    unit/cpu/cpu_test.cpp
)
target_link_libraries(cpu_test GTest::gtest_main fcemu)
add_test(NAME cpu_test COMMAND cpu_test)

# PPU 测试
add_executable(ppu_test
    unit/ppu/ppu_test.cpp
)
target_link_libraries(ppu_test GTest::gtest_main fcemu)
add_test(NAME ppu_test COMMAND ppu_test)

# APU 测试
add_executable(apu_test
    unit/apu/apu_test.cpp
)
target_link_libraries(apu_test GTest::gtest_main fcemu)
add_test(NAME apu_test COMMAND apu_test)

# 内存测试
add_executable(memory_test
    unit/memory/memory_test.cpp
)
target_link_libraries(memory_test GTest::gtest_main fcemu)
add_test(NAME memory_test COMMAND memory_test)
