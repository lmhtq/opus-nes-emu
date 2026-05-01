# FEAT-116: AI 超分 PoC（Mac / Real-ESRGAN ncnn 子进程）

## 元数据 (Metadata)

- **ID**: FEAT-116
- **关联模块 (Related Module)**: MOD-VIDEO-AIUPSCALE
- **关联需求 (Related Requirements)**: REQ-116
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-05-01

## 功能描述 (Feature Description)

提供 `IAiUpscaler` 抽象接口与两个后端实现，并提供 `fcemu_ai_upscale_demo` 工具
完成"ROM → 60 帧 PPU 输出 → AI 4× 超分 → side-by-side 对比 PNG + 分阶段耗时"
的完整 PoC 链路。

## 接口定义 (Interface Definition)

见 `src/include/fcemu/ai_upscaler.h`（详见 MOD-VIDEO-AIUPSCALE）。

工具 CLI：

```
fcemu_ai_upscale_demo <rom> [options]
  --model NAME            默认 realesr-animevideov3
  --model-dir DIR         默认 $FCEMU_AIUP_MODEL_DIR 或 ./models
  --bin PATH              默认 $FCEMU_AIUP_BIN 或 PATH 中 realesrgan-ncnn-vulkan
  --frames N              boot 后跑的总帧数（默认 120）
  --capture-from N        从第 N 帧开始抓（默认 60，跳过启动黑屏）
  --capture-count N       抓取帧数（默认 1）
  --every K               每 K 帧抓一次（默认 1）
  --out-dir DIR           输出目录（默认 ./aiup_out）
  --backend NAME          nearest | ncnn-subprocess（默认 ncnn-subprocess）
  --keep-temp             保留中间 tmpdir
```

## 流程图 (Flow Chart)

```
[CLI 解析]
   │
   ▼
[加载 ROM → boot CPU/PPU/APU/Mem (复用 headless_runner)]
   │
   ▼
[run frames; 在指定帧抓取 PPU.frame().pixels]
   │
   ▼
[Frame[] in-memory]
   │
   ├──► 写 PNG 到 tmp/in/frame_000001.png …    (timing: write_png_ms)
   │
   ▼
[fork+exec realesrgan-ncnn-vulkan -i tmp/in -o tmp/out -n MODEL -s SCALE -m MODEL_DIR]
                                                (timing: subprocess_ms)
   │
   ▼
[读 PNG tmp/out/frame_000001.png …]            (timing: read_png_ms)
   │
   ▼
[生成对比图：左=Nearest 4×，右=AI 输出]         (timing: montage_ms)
   │
   ▼
[打印分阶段耗时表 + 写 timing.json]
```

## 边界条件 (Edge Cases)

1. ROM 文件无法读取 → 退出码 2
2. 二进制/模型缺失 → 退出码 3，stderr 给出探测过程
3. 子进程超时（默认 60 s）→ 终止并返回 false
4. PNG 写盘失败（磁盘满） → 返回 false
5. 输出帧少于输入 → 视为失败，保留 tmpdir 供排查
6. 帧 RGBA 含非 255 alpha → 写 PNG 前强制覆盖为 255

## 测试场景 (Test Scenarios)

| ID | 场景 | 期望 |
|----|------|------|
| TS-1 | NearestNeighborUpscaler 4× | 输出 1024×960，与 stb 最近邻一致 |
| TS-2 | NcnnSubprocessUpscaler init 缺失 binary | 返回 false，错误信息含 "binary not found" |
| TS-3 | NcnnSubprocessUpscaler 单帧 SMB title | 输出 1024×960，文件头为 PNG |
| TS-4 | 批量 16 帧 | 顺序与输入一致，所有输出尺寸正确 |
| TS-5 | end-to-end demo on SMB | 生成 montage_*.png 与 timing.json |
| TS-6 | timing 拆分合理 | 4 个阶段耗时之和 ≤ 总耗时 + 5% |

## 性能预算（M2 base，60 帧 ×4）

| 阶段 | 预算 |
|------|------|
| write_png | ≤ 200 ms |
| subprocess (ncnn 推理) | ≤ 4500 ms |
| read_png | ≤ 600 ms |
| montage | ≤ 300 ms |
| 合计 | ≤ 6 s（验收标准 #10） |

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/ppu/rendering.md`

## 后端实测对比（M2 base，SMB title）

| 后端 | 模型 | scale | 单帧耗时 | worker 吞吐 | 主循环 fps | 备注 |
|------|------|-------|---------|------------|-----------|------|
| ncnn-subprocess | animevideov3 | 4 | ~510 ms | ~1.85 fps | 60 | 子进程冷启动是主要开销 |
| ncnn-inprocess  | animevideov3 | 4 | ~285 ms | ~3.5 fps  | 40-48 | MoltenVK 直链，模型常驻 |
| ncnn-inprocess  | animevideov3 | 2 | ~120-280 ms | ~6-8 fps  | 20-40 | 输出 512×480 |
| **coreml (ANE)** | animevideov3 | 4 | **~60 ms (含 CHW conv)** / 9.5 ms 纯推理 | **~17 fps** | **60+** | Apple Neural Engine（标量 conv 版本） |
| **coreml + vImage** | animevideov3 | 4 | **~43 ms** | **~23 fps** | **60+** | RGBA↔CHW 走 Accelerate vImage SIMD |
| **coreml + vImage + NEON F16→RGBA + 输出回收 🟢** | animevideov3 | 4 | **~13 ms** | **~60 fps（达成 1:1）** | **60+** | F16→RGBA 单遍 NEON kernel（去掉 F32 中转）+ 缓存 MLMultiArray + outputBackings + 输出 buffer 池（消除每帧 4.9MB 页表 fault） |

CoreML 路径（`src/video/ai_upscaler_coreml.mm`）：
- PyTorch → coremltools → `.mlpackage` → `xcrun coremlcompiler compile` → `.mlmodelc`，输入张量 baked 为 `(1,3,240,320)`（fcemu VideoEnhancer 输出尺寸）。
- `MLComputeUnitsAll`：实测会优先调度到 ANE。
- 当前瓶颈是 RGBA8 ↔ float32 CHW 的标量转换（输出 1280×960×3 共 ~3.7M 元素）；纯模型推理 9.5 ms / 105 fps。后续可用 vImage/Accelerate 向量化。
- 模型文件命名约定：`<model>-x<scale>.mlmodelc`，例 `realesr-animevideov3-x4.mlmodelc`。

`dlopen libvulkan.1.dylib` 警告是 ncnn 先试 Vulkan loader 再回落 MoltenVK 的正常路径，不影响功能。

## 实时（在线）集成 — 第二阶段 (SDL/Metal Live Path)

PoC 工具验证完算法可用性后，将其接入主程序 `src/main.cpp` 的渲染循环。
关键点：

1. **AsyncUpscaleService**（`src/include/fcemu/ai_upscale_service.h`）
   - worker 线程持有 `IAiUpscaler`；主线程 `submit()` 非阻塞，新输入覆盖未消费的旧输入（背压：丢中间帧）
   - `try_get_latest()` 返回 generation id，用于判定"是否是新输出"
2. **主循环改造**（`src/main.cpp`）
   - 命令行 `--ai-upscale[=<model>]` `--ai-scale=N` 启用
   - 每帧把 VideoEnhancer 输出 submit 给 service
   - SDL 纹理始终按 `target_w × target_h` 创建（避免每帧重建）：
     - 有最新 AI 输出 → 拷贝
     - 无 → 主线程做最近邻放大到 target，保持画面流畅
3. **SDL/Metal**（`src/ui/ui.cpp`）
   - macOS 下 `SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal")`
   - 失败回退到 software，避免 headless / dummy 驱动场景崩溃
   - 启动时打印实际 renderer 名称
4. **HUD**：`AIUP <ema>ms drop=N ok=N` 实时反映吞吐
5. **观测脚本**：每 2 秒 stdout 打印 submitted/dropped/processed/failed/last/ema

**M2 base 实测（SDL_VIDEODRIVER=dummy 烟雾测试 18 秒）**：

| 指标 | 值 |
|------|----|
| 模拟器主循环 | 60.5 fps（不被 AI 阻塞）|
| AI worker 吞吐 | ~1.85 fps（subprocess ~510 ms/帧）|
| 丢帧率 | ~97% （back-pressure 正确生效）|
| 失败 | 0 |
| 主线程渲染源 | 有 AI 输出 → AI 帧；其他 → 最近邻 fallback |

**已知限制**：
- subprocess 模型每次冷启动 ~500 ms，难以满足 60 fps 实时；下一阶段需切换为 in-process ncnn-vulkan 直链调用
- HUD/menu/toast 仍按目标分辨率叠加 → 文字小但锐利（可接受）

## 变更记录 (Change History)

- 2026-05-01: Initial version
- 2026-05-01: SDL/Metal 实时集成（AsyncUpscaleService + main loop hookup）
- 2026-05-01: ncnn-vulkan in-process 后端（`src/video/ai_upscaler_ncnn.cpp`）；M2 + MoltenVK，吞吐相比子进程提升 ~2×（285 vs 510 ms/帧）。
- 2026-05-01: CoreML/ANE 后端（`src/video/ai_upscaler_coreml.mm` + `models/realesr-animevideov3-x4.mlmodelc`）；M2 ANE 单帧 ~60 ms（端到端含 CHW 转换）/ 9.5 ms（纯推理），主循环稳定 60+ fps。
- 2026-05-01: CoreML 路径 RGBA↔CHW 用 Accelerate vImage 向量化；端到端 60→43 ms（−28%），worker 17→23 fps。模型同时支持 256×240 和 320×240 输入（EnumeratedShapes）。
- 2026-05-01: CoreML 路径再优化至 **13 ms / 60 fps**（与主循环 1:1 同步）。三处关键改动：
  1. 缓存 `MLMultiArray` / `MLDictionaryFeatureProvider` / `MLPredictionOptions`，并用 `outputBackings` 把模型直接写进我们持有的 buffer，省一次内部 alloc + memcpy；
  2. F16 输出走单遍 NEON kernel（`vmulq_f16`/`vcvtq_u16_f16`/`vst4_u8`）直出 RGBA8，跳过 F16→F32→U8→interleave 的多 pass；
  3. 在 `IAiUpscaler` 加 `recycle_output_buffer()`，worker 每帧把消费完的 RGBA buffer 还回 upscaler 的 pool，**消除 4.9 MB 缓冲每帧首次写入的 ~27 ms 页表 fault**（先前最大瓶颈）。同时 `Frame::rgba` 改为带 `NoInitAllocator` 的 `ByteVec`，跳过 `std::vector::resize` 的零填充。
