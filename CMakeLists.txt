cmake_minimum_required(VERSION 3.16)
project(fcemu VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# 源文件
set(SOURCES
    src/cpu/cpu.cpp
    src/ppu/ppu.cpp
    src/apu/apu.cpp
    src/memory/memory.cpp
    src/cartridge/cartridge.cpp
    src/input/input.cpp
    src/ui/ui.cpp
    src/video/video_enhancer.cpp
    src/audio/audio_enhancer.cpp
    src/haptics/haptics.cpp
    src/replay/replay.cpp
    src/resource/resource_analyzer.cpp
    src/presets/presets.cpp
    src/main.cpp
)

# 头文件目录
include_directories(include)
include_directories(${SDL2_INCLUDE_DIRS})

# 可执行文件
add_executable(fcemu ${SOURCES})

# 链接库
target_link_libraries(fcemu
    ${SDL2_LIBRARIES}
    OpenGL::GL
)

# 测试
enable_testing()
add_subdirectory(tests)

# 安装
install(TARGETS fcemu DESTINATION bin)
