# REQ-116: AI 实时超分（Mac PoC）

## 元数据 (Metadata)

- **ID**: REQ-116
- **状态 (Status)**: Draft
- **优先级 (Priority)**: High
- **创建日期 (Created)**: 2026-05-01
- **最后更新 (Updated)**: 2026-05-01
- **作者 (Author)**: Copilot

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/ppu/rendering.md`（256×240 RGBA 输出来源）

## 需求描述 (Requirement Description)

为 fcemu 引入"AI 超分"通道，将 PPU 的 256×240 原始画面经现代深度学习超分模型
重建到 4×（1024×960）输出，让 FC 像素游戏在现代显示器上具备"重制"级观感。

后端解耦设计为可插拔：

| 平台 | 主后端 | 备份/低延迟后端 | 后续 |
|------|--------|------------------|------|
| **Mac (Apple Silicon)** | Real-ESRGAN **animevideov3** (ncnn + Vulkan/MoltenVK→Metal) | Anime4K Metal Shader | Core ML / ANE |
| **NVIDIA** | Real-ESRGAN TensorRT (FP16/INT8) | DLSS Spatial | RTX VSR |

本需求覆盖 **Mac 后端的 PoC（阶段 1）**：通过子进程调用预编译的
`realesrgan-ncnn-vulkan` 二进制，验证全链路（ROM→PPU→AI 超分→对比图）可行性
与画质收益。**实时窗口播放、ncnn 直链、CoreML/ANE 路径放在阶段 2 及以后。**

## 功能要求

1. 提供统一的 `IAiUpscaler` C++ 抽象接口，支持单帧与批量两种调用形式
2. 实现两个后端：
   - `NearestNeighborUpscaler`：纯 CPU 最近邻，零依赖，作为基线/兜底/单测对照
   - `NcnnSubprocessUpscaler`：调用外部 `realesrgan-ncnn-vulkan` 完成 4× 推理
3. 提供命令行工具 `fcemu_ai_upscale_demo`：
   - 输入：ROM、模型名、帧数、采样间隔、输出目录
   - 输出：原始帧序列、超分帧序列、side-by-side 对比 PNG、分阶段耗时报告
4. 在追溯矩阵中登记 REQ-116 → MOD-VIDEO-AIUPSCALE → FEAT-116 → 实现/测试
5. 二进制与模型采用"运行时探测"，不进入构建期依赖（缺失给出明确错误信息）

## 验收标准 (Acceptance Criteria)

1. 接口头 `fcemu/ai_upscaler.h` 编译通过，提供 `IAiUpscaler`、`Frame`、
   `UpscalerCaps`、`UpscalerConfig` 等公开类型
2. `NearestNeighborUpscaler` 单帧 256×240→1024×960 输出像素与裸 4× 复制一致
3. `NcnnSubprocessUpscaler::init()` 在缺失二进制时返回 false 并给出 stderr 提示，
   不应导致构建或调用方崩溃
4. `fcemu_ai_upscale_demo` 可对 SMB ROM 在 Mac 上跑通端到端，输出至少一张
   原始 vs 超分对比 PNG，并打印分阶段耗时（PNG 写 / 推理子进程 / PNG 读 / 总）
5. 模型探测顺序：CLI 参数 > 环境变量 `FCEMU_AIUP_MODEL_DIR` > 默认 `./models`
6. 二进制探测顺序：CLI 参数 > 环境变量 `FCEMU_AIUP_BIN` > `PATH` 中
   `realesrgan-ncnn-vulkan`
7. PoC 阶段不修改 `VideoEnhancer::process()` 在线路径；在线路径下集成留待
   阶段 2（异步调度器 + GPU 直链）
8. PNG 通道：写入时强制 alpha=255；读取时忽略输出 alpha 通道
9. 文档：REQ-116、MOD-VIDEO-AIUPSCALE、FEAT-116、追溯矩阵均完成
10. M2 base 上 60 帧 256×240→1024×960 端到端用时 < 6 s（基准 ~3.9 s）

## 非目标 (Out of Scope)

- 实时（≥30 fps）窗口播放（阶段 2）
- ncnn / CoreML 直链（阶段 2/3）
- 异步帧调度器、回压、最近一帧 fallback（阶段 2）
- NVIDIA 后端（阶段 3）
- AI 超分与 CRT/HDR/宽屏的精确顺序与协同（阶段 2 设计）
- 模型/二进制的签名、公证、Gatekeeper 处理（阶段 4 打包）

## 性能基准（参考，M2 base）

| 模型 | 倍率 | 60 帧端到端 | 模型大小 |
|------|------|-------------|----------|
| realesr-animevideov3 | ×4 | ~3.9 s（≈15 fps，含 PNG IO） | 2.4 MB |
| realesrgan-x4plus-anime | ×4 | ~80 s（≈0.7 fps） | 17 MB |
| realesrgan-x4plus | ×4 | ~160 s（≈0.4 fps） | 64 MB |

## 依赖需求 (Dependencies)

- REQ-002（PPU 渲染：提供 256×240 RGBA 源）
- REQ-101（画质增强：未来阶段 2 的管线协同）

## 标签 (Tags)

#video #ai #upscale #mac #metal #ncnn

## 备注 (Notes)

- Mac 推理后端选型对比详见 `docs/feature-design/FEAT-116.md`
- 模型与二进制许可证：Real-ESRGAN 为 BSD-3-Clause（模型同许可），可重分发
  但需保留版权声明
- 后续计划：阶段 2 引入 ncnn 静态库直链 + Metal/MoltenVK 复用 GPU 纹理，目标
  M2 base 60 fps（×2）/ 30 fps（×4）；阶段 3 加 CoreML/ANE
