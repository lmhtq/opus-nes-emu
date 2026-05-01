cmake_minimum_required(VERSION 3.16)
project(fcemu VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# 可选：ncnn (REQ-116 in-process AI upscale 后端)。
# brew install ncnn 即可。未安装时自动跳过该后端。
# ncnn 的 cmake config 会硬要求 OpenMP::OpenMP_CXX 目标存在（即使 brew ncnn 未启用 OpenMP），
# 这里需要手动定位 libomp 并给 OpenMP_CXX 提供 IMPORTED 目标。
if(APPLE)
    find_path(LIBOMP_INCLUDE_DIR omp.h
        HINTS /opt/homebrew/opt/libomp/include /usr/local/opt/libomp/include)
    find_library(LIBOMP_LIBRARY NAMES omp
        HINTS /opt/homebrew/opt/libomp/lib /usr/local/opt/libomp/lib)
    if(LIBOMP_INCLUDE_DIR AND LIBOMP_LIBRARY AND NOT TARGET OpenMP::OpenMP_CXX)
        add_library(OpenMP::OpenMP_CXX UNKNOWN IMPORTED)
        set_target_properties(OpenMP::OpenMP_CXX PROPERTIES
            IMPORTED_LOCATION "${LIBOMP_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBOMP_INCLUDE_DIR}"
            INTERFACE_COMPILE_OPTIONS "-Xpreprocessor;-fopenmp")
    endif()
endif()
find_package(ncnn QUIET CONFIG)
if(ncnn_FOUND)
    message(STATUS "ncnn found: in-process AI upscaler enabled")
else()
    message(STATUS "ncnn NOT found: only subprocess/nearest AI upscaler available")
endif()

# 库源文件（不含 main.cpp）
set(LIB_SOURCES
    src/cpu/cpu.cpp
    src/ppu/ppu.cpp
    src/apu/apu.cpp
    src/memory/memory.cpp
    src/cartridge/cartridge.cpp
    src/cartridge/mappers.cpp
    src/input/input.cpp
    src/ui/ui.cpp
    src/video/video_enhancer.cpp
    src/video/ai_upscaler.cpp
    src/video/ai_upscale_service.cpp
    src/audio/audio_enhancer.cpp
    src/haptics/haptics.cpp
    src/replay/replay.cpp
    src/resource/resource_analyzer.cpp
    src/presets/presets.cpp
    src/savestate/savestate.cpp
    src/social/social_bridge.cpp
    src/overlay/overlay.cpp
    src/menu/menu.cpp
)
if(ncnn_FOUND)
    list(APPEND LIB_SOURCES src/video/ai_upscaler_ncnn.cpp)
endif()
if(APPLE)
    list(APPEND LIB_SOURCES src/video/ai_upscaler_coreml.mm)
    set_source_files_properties(src/video/ai_upscaler_coreml.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc")
endif()

# 头文件目录
include_directories(src/include)
include_directories(third_party/stb)
include_directories(${SDL2_INCLUDE_DIRS})

# 静态库
add_library(fcemu_lib STATIC ${LIB_SOURCES})
target_include_directories(fcemu_lib PUBLIC src/include ${SDL2_INCLUDE_DIRS})
target_link_libraries(fcemu_lib PUBLIC ${SDL2_LIBRARIES})
if(ncnn_FOUND)
    target_link_libraries(fcemu_lib PUBLIC ncnn)
    target_compile_definitions(fcemu_lib PUBLIC FCEMU_HAVE_NCNN=1)
endif()
if(APPLE)
    target_link_libraries(fcemu_lib PUBLIC
        "-framework CoreML"
        "-framework Foundation")
    target_compile_definitions(fcemu_lib PUBLIC FCEMU_HAVE_COREML=1)
endif()

# 可执行文件
add_executable(fcemu src/main.cpp)
target_link_libraries(fcemu
    fcemu_lib
    ${SDL2_LIBRARIES}
    OpenGL::GL
)

# 无 SDL 依赖的 headless 跑分/冒烟工具
add_executable(fcemu_headless tools/headless.cpp)
target_link_libraries(fcemu_headless fcemu_lib)

# REQ-116 PoC: AI 实时超分 demo（Mac 后端）
add_executable(fcemu_ai_upscale_demo tools/ai_upscale_demo.cpp)
target_include_directories(fcemu_ai_upscale_demo PRIVATE third_party/stb)
target_link_libraries(fcemu_ai_upscale_demo fcemu_lib)

# 测试
enable_testing()
add_subdirectory(tests)

# 安装
install(TARGETS fcemu DESTINATION bin)
