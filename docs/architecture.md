# fcemu 架构总览

> 本文按 `src/main.cpp` 的实际数据流绘制，呈现各模块在每帧 60 fps 主循环里的协作关系。
> 与 8 阶段文档（specs / overview / module-design / feature-design）正交：
> 这里给出"系统在跑时长什么样"的全景图，便于新人在 5 分钟内建立心智模型。

## ASCII 架构图

```
                                  fcemu  架构总览
                          (C++17 / SDL2 / CMake / macOS·Linux)

┌──────────────────────── 外部资源 (磁盘 / 环境) ──────────────────────────────┐
│  ROM (.nes)   fcemu.ini   <rom>.state   <rom>.sav   presets/   models/      │
│                                              social.watch_file              │
│                  fcemu-photo/{<rom>_<ts>.png,<rom>_<ts>_orig.png}            │
│                                          (+ photo daemon UNIX socket)       │
└──┬──────────┬────────────┬──────────────────┬───────────────┬────────┬──────┘
   │          │            │                  │               │        │
   ▼          ▼            ▼                  ▼               ▼        ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                          src/main.cpp  (主循环 60 fps)                       │
│   每帧：events → input → CPU step×N → PPU/APU → enhance → AI → SDL present  │
└──┬──────────────────────────────────────────────────────────────────────────┘
   │   wiring (callbacks)
   ▼
┌────────────────────────── CORE  (FC 硬件模拟，src/) ────────────────────────┐
│                                                                              │
│   ┌────────┐  read/write   ┌────────┐  cpu_read/write   ┌──────────────┐    │
│   │ CPU    │ ◄───────────► │ Memory │ ◄──────────────►  │ PPU  256×240 │    │
│   │ 6502   │   NMI/IRQ ◄── │  bus   │   OAM DMA ─────►  │ + CHR/VRAM   │    │
│   └───┬────┘               └───┬────┘                   └──────┬───────┘    │
│       │                        │                               │            │
│       │                        ├──► APU (44.1 kHz, DMC reader) │            │
│       │                        ├──► Input  (P1/P2, turbo)      │            │
│       │                        └──► Cartridge ◄────────────────┘            │
│       │                              + Mappers                              │
│       │                              + battery RAM / CHR-RAM                │
│       │                                                                     │
│       └── SaveState  (Serializer: CPU+MEM+PPU+APU+Cart, magic 'FCES')       │
└──────────────────────────────────────────────────────────────────────────────┘
            │ ppu.frame().pixels (256×240 RGBA)        │ apu samples (s16)
            ▼                                          ▼
┌──────────── 增强层 (现代声光电体验, src/video, audio, haptics) ──────────────┐
│                                                                              │
│  VideoEnhancer ──► AI Upscale Service (异步) ──► render_buf                  │
│   • CRT 扫描线/曲率/HDR        │      ┌─ CoreML  (Metal/ANE, .mlmodelc)       │
│   • Widescreen 256→320         ├─ 后端 ┼─ NCNN in-process                    │
│   • hit-flash / shake          │      └─ NCNN subprocess                    │
│   • 视觉特效                   │   10+ 模型: realcugan / realesrgan / ...   │
│                                                                              │
│  AudioEnhancer                                                               │
│   • 立体声/3D/EQ/混响                                                        │
│   • 场景自适应  RMS→{calm,menu,action,boss}    每 30 帧重判                  │
│   • remix oneshot (外部 PCM)                                                 │
│                                                                              │
│  HapticsManager   每帧从画面采主色 → RGB callback / 震动 / 自适应扳机        │
└──────────────────────────────────────────────────────────────────────────────┘
            │ render_ptr (w×h RGBA)
            ▼
┌──────────── 表现层 (src/ui, menu, overlay, presets) ─────────────────────────┐
│                                                                              │
│   UI (SDL2 window / GL / audio queue / key+pad input / settings I/O)         │
│    │                                                                         │
│    ├── Menu       ESC·F4 打开，暂停 CPU；Video/Audio/Haptics/Controls/Quit   │
│    ├── Overlay    HUD lines + Toast 队列 (font8x8_basic 内置位图字体)        │
│    └── Settings   fcemu.ini  (即时落盘, 键位绑定, 冲突检测)                  │
│                                                                              │
│   PresetManager  按 ROM sha256 匹配 presets/{video,audio}/                   │
└──────────────────────────────────────────────────────────────────────────────┘
            │
            ▼
┌──────────── 外围扩展 (src/social, replay, resource, savestate) ──────────────┐
│   SocialBridge       监听文件 → Cheer/Shake/Gift/Vote/Chat                  │
│                      → 触发 venh.flash/shake + haptics + overlay toast       │
│   ReplayManager      录制 FrameData (高光回放, REQ-114)                      │
│   ResourceAnalyzer   可替换资源提取 (REQ-115)                                │
│   Photo Mode  (P)    PPU 帧 PNG → UNIX socket → 云端生图 API daemon          │
│                      tools/photo/photo_repaint.py  (Ark / OpenAI / ...)      │
└──────────────────────────────────────────────────────────────────────────────┘

┌──────────── 文档与流程 (8 阶段) ─────────────────────────────────────────────┐
│ docs/hardware → specs/REQ-XXX → overview → module-design → feature-design    │
│        → src/ → tests/{unit,e2e,integration} → docs/traceability/matrix.md   │
└──────────────────────────────────────────────────────────────────────────────┘
```

## 关键观察

- **主循环单线程驱动**：`main.cpp` 每帧串起 input → CPU/PPU/APU → VideoEnhancer → AI upscale → SDL present。AI 推理走异步 worker（`AsyncUpscaleService`），主循环只 `submit` / `try_get_latest`，掉帧时复用最近一次结果，因此即便后端慢也不会卡 60 fps。
- **Memory 是总线中枢**：CPU 只持有 `read/write` 两个 callback，Memory 再分发到 PPU/APU/Input/Cartridge，并负责 OAM DMA。这是这套架构能保持模块解耦的关键。
- **增强层是叠加式的**：PPU 原始 256×240 始终保留（Photo Mode / Replay 都用原始帧），CRT、宽屏、AI upscale 链式追加在后面。
- **AI 后端可插拔**：CoreML / NCNN-inproc / NCNN-subprocess 三选一，`UpscalerConfig` + `make_*_upscaler()` 工厂统一接口。
- **Photo Mode 是进程外**：C++ 写 PNG 到 `/tmp`，通过 UNIX socket 把 JSON 请求丢给常驻的 Python SDXL daemon，结果通过 toast 队列回流主循环——避免把 8 GiB 模型拉进模拟器进程。

## 与 8 阶段文档的对应

| 本图分层 | 主要源码目录 | 对应模块设计 (MOD) | 对应需求 (REQ) |
|----------|--------------|---------------------|----------------|
| CORE | `src/cpu`, `ppu`, `apu`, `memory`, `cartridge`, `input` | MOD-CPU / MOD-PPU / MOD-APU / ... | REQ-001 ~ REQ-020 |
| 增强层 | `src/video`, `src/audio`, `src/haptics` | 视觉/音频/触觉模块 | REQ-101 ~ REQ-112, REQ-116 |
| 表现层 | `src/ui`, `src/menu`, `src/overlay`, `src/presets` | UI / Menu / Overlay | REQ-010, REQ-105, REQ-110 |
| 外围扩展 | `src/social`, `src/replay`, `src/resource`, `src/savestate`, `tools/photo` | Social / Replay / Resource | REQ-007, REQ-113 ~ REQ-115, REQ-117 |
