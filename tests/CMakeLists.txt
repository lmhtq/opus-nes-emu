# tests/CMakeLists.txt
enable_testing()

set(UNIT_TESTS
    cpu/cpu_test
    ppu/ppu_test
    apu/apu_test
    memory/memory_test
    cartridge/cartridge_test
    input/input_test
    replay/replay_test
    presets/presets_test
    savestate/savestate_test
    social/social_test
    audio/scene_test
    video/widescreen_test
)

foreach(t ${UNIT_TESTS})
    string(REPLACE "/" "_" name ${t})
    add_executable(${name} unit/${t}.cpp)
    target_link_libraries(${name} fcemu_lib)
    add_test(NAME ${name} COMMAND ${name})
endforeach()

# E2E
add_executable(e2e_boot_test e2e/boot_test.cpp)
target_link_libraries(e2e_boot_test fcemu_lib)
add_test(NAME e2e_boot_test COMMAND e2e_boot_test)
