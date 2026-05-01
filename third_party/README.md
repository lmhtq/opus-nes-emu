# third_party

外部依赖（vendored，源码内嵌）。

## stb/

- 来源：https://github.com/nothings/stb
- 许可：Public Domain (Unlicense) 或 MIT，任选其一
- 用途：fcemu 中 AI 超分 PoC 工具用于 PNG 编解码（避免引入 libpng 依赖）
- 具体头文件：
  - `stb_image.h` v2.30
  - `stb_image_write.h`
- 实现宏 `STB_IMAGE_IMPLEMENTATION` / `STB_IMAGE_WRITE_IMPLEMENTATION` 仅在
  `src/video/ai_upscaler.cpp` 内定义一次。
