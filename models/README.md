# models/

CoreML 编译版（`.mlmodelc`）超分模型，供 fcemu A/B 档实时管线 (`--ai-mode` /
`--ai-upscale`) 加载。所有模型走 ANE / Metal。

## 文件清单

| mlmodelc | scale | 体积 | M2 ANE 单帧 | 风格 / 来源 |
|----------|-------|------|-------------|-------------|
| `realesr-animevideov3-x4.mlmodelc`   | x4 | ~1.5 MiB | 11 ms | 动漫视频，RealESR 官方紧凑模型 |
| `realcugan-denoise3x-x4.mlmodelc`    | x4 | ~5 MiB   | 22 ms | **默认（fast 档）**，RealCUGAN 强去噪 |
| `realcugan-no-denoise-x4.mlmodelc`   | x4 | ~5 MiB   | 22 ms | RealCUGAN 保留颗粒 |
| `realcugan-conservative-x4.mlmodelc` | x4 | ~5 MiB   | 22 ms | RealCUGAN 折中 |
| `realesr-general-x4v3-x4.mlmodelc`     | x4 | ~5 MiB | 28 ms | 通用画面 |
| `realesr-general-wdn-x4v3-x4.mlmodelc` | x4 | ~5 MiB | 28 ms | 通用 + 弱去噪 |
| `apisr-rrdb-x4.mlmodelc`             | x4 | ~10 MiB  | 50 ms | APISR 动漫专用 |
| `realesrgan-anime-6b-x4.mlmodelc`    | x4 | ~17 MiB  | 58 ms | **medium 档**，RealESRGAN-anime 6 块 |
| `animejanai-sd-x2.mlmodelc`          | x2 | ~6 MiB   | 30 ms | AnimeJaNai SD（标清动漫） |
| `animejanai-hd-v3-x2.mlmodelc`       | x2 | ~6 MiB   | 35 ms | AnimeJaNai HD v3 |
| `animejanai-hd-v3-sharp1-x2.mlmodelc`| x2 | ~6 MiB   | 35 ms | AnimeJaNai HD v3 加锐 |
| `realesrgan-x4plus-x4.mlmodelc`      | x4 | ~64 MiB  | 185 ms | **quality 档**，RealESRGAN x4plus，画质最强 |

`fast / medium / quality` 三档与默认模型映射在 `src/main.cpp` 中。

## 来源 & 转换

原始权重（pth）来自各项目官方 release：
- Real-ESRGAN: <https://github.com/xinntao/Real-ESRGAN>
- Real-CUGAN: <https://github.com/bilibili/ailab/tree/main/Real-CUGAN>
- APISR:       <https://github.com/Kiteretsu77/APISR>
- AnimeJaNai:  <https://github.com/the-database/mpv-upscale-2x_animejanai>

转换流程：`PyTorch → ONNX → coremltools (Float16, MLProgram, ANE) → mlmodelc`。
fcemu 仓库直接保存 `.mlmodelc`（约 130 MiB 总计）以避免开发者重新转换。
若要新增模型，参考 `tools/ai_upscale_demo.cpp` 中的输入输出张量约定
（`NCHW float16`，channel=3，输入归一化到 0-1）。

## 加载

由 `src/video/ai_upscaler_coreml.mm` 通过 `MLModel modelWithContentsOfURL:`
加载，按 enumerated shape 自动 padding 到模型支持的最近输入尺寸。
