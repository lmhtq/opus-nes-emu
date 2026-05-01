# 追溯矩阵 (Traceability Matrix)

本文档维护 fcemu 项目从需求到实现/测试的端到端追溯关系。
每完成一个阶段时同步更新对应行的状态。

## 追溯链 (Chain)

```
docs/hardware/* → docs/specs/REQ-*.md
              → docs/overview/OVERVIEW-*.md
                  → docs/module-design/MOD-*.md
                      → docs/feature-design/FEAT-*.md
                          → src/<module>/
                              → tests/unit/<module>/
                                  → tests/e2e/
                                      → tests/integration/
```

## 基础需求 (REQ-001 ~ REQ-010)

| Req | 描述 | Overview | Module | Feature | 实现 | 单元测试 | 状态 |
|-----|------|----------|--------|---------|------|----------|------|
| REQ-001 | 6502 CPU 模拟 | OVERVIEW-001 | MOD-CPU | FEAT-001 | `src/cpu/cpu.cpp` | `tests/unit/cpu/` | ✅ 实现 |
| REQ-002 | PPU 渲染 | OVERVIEW-001 | MOD-PPU | FEAT-002 | `src/ppu/ppu.cpp` | `tests/unit/ppu/` | ✅ 实现 |
| REQ-003 | APU 音频合成 | OVERVIEW-001 | MOD-APU | FEAT-003 | `src/apu/apu.cpp` | `tests/unit/apu/` | ✅ 实现 |
| REQ-004 | 内存与总线 | OVERVIEW-001 | MOD-MEMORY | FEAT-004 | `src/memory/memory.cpp` | `tests/unit/memory/` | ✅ 实现 |
| REQ-005 | iNES 加载 + 电池 RAM | OVERVIEW-001 | MOD-CARTRIDGE | FEAT-005 | `src/cartridge/cartridge.cpp` | `tests/unit/cartridge/` | ✅ 实现 |
| REQ-006 | 输入设备 | OVERVIEW-001 | MOD-INPUT | FEAT-006 | `src/input/input.cpp` | `tests/unit/input/` | ✅ 实现 |
| REQ-007 | 存档/读档 (Save State) | OVERVIEW-001 | MOD-MEMORY/MOD-CARTRIDGE | — | `src/savestate/savestate.cpp` + `src/main.cpp` (F1/F2) | `tests/unit/savestate/` | ✅ 实现 |
| REQ-008 | Mapper 0–4 | OVERVIEW-001 | MOD-CARTRIDGE | — | `src/cartridge/mappers.cpp` | `tests/unit/cartridge/` | ✅ 实现 |
| REQ-009 | 配置持久化 | OVERVIEW-001 | MOD-UI | — | `src/ui/ui.cpp` (settings ini) + 菜单内即时落盘 | `tests/unit/menu/` | ✅ 实现 |
| REQ-010 | Debug/性能 | OVERVIEW-001 | MOD-UI/MOD-OVERLAY | — | `src/ui/ui.cpp` (F3/Tab) + `src/overlay/overlay.cpp` (HUD) | `tests/unit/overlay/` | ✅ 实现 |
| REQ-011 | 输入设备配置（手柄/连发） | OVERVIEW-001 | MOD-INPUT/MOD-UI | — | `src/input/input.cpp` + `src/ui/ui.cpp` + 菜单 Controls 子菜单 | `tests/unit/input/`, `tests/unit/menu/` | ✅ 实现 |
| REQ-012 | 现代化交互 (GUI 菜单 / Toast / HUD) | OVERVIEW-001 | MOD-OVERLAY/MOD-MENU | — | `src/overlay/overlay.cpp` + `src/menu/menu.cpp` + `src/ui/ui.cpp` + `src/main.cpp` | `tests/unit/overlay/`, `tests/unit/menu/` | ✅ 实现 |

## 增强需求 (REQ-101 ~ REQ-115)

| Req | 描述 | Module | 实现 | 状态 |
|-----|------|--------|------|------|
| REQ-101 | 画质增强 (CRT/HDR/AA) | MOD-VIDEO-ENHANCER | `src/video/video_enhancer.cpp` | ✅ CRT/HDR |
| REQ-102 | 高清纹理替换 | MOD-VIDEO-ENHANCER | `src/video/video_enhancer.cpp` + `src/include/fcemu/ppu.h` (TileOverrideFn) | ⚠️ 钩子就绪 |
| REQ-103 | 宽屏补丁 | MOD-VIDEO-ENHANCER | `src/video/video_enhancer.cpp` (320x240 输出) | ✅ 实现 |
| REQ-104 | 视觉特效 (shake/flash) | MOD-VIDEO-ENHANCER | `src/video/video_enhancer.cpp` | ✅ 实现 |
| REQ-105 | 预制视觉包 | MOD-PRESETS | `src/presets/presets.cpp` | ✅ 实现 |
| REQ-106 | 立体声 / 均衡器 | MOD-AUDIO-ENHANCER | `src/audio/audio_enhancer.cpp` | ✅ 实现 |
| REQ-107 | 动态音效联动 | MOD-AUDIO-ENHANCER | `src/audio/audio_enhancer.cpp` (set_scene + RMS 自动切换) | ✅ 实现 |
| REQ-108 | 音频可视化 | MOD-AUDIO-ENHANCER | `src/audio/audio_enhancer.cpp` (vis) | ✅ 实现 |
| REQ-109 | 音频替换 (remix) | MOD-AUDIO-ENHANCER | `src/audio/audio_enhancer.cpp` (load + mix-in) | ✅ 实现 |
| REQ-110 | 预制音频包 | MOD-PRESETS | `src/presets/presets.cpp` | ✅ 实现 |
| REQ-111 | 手柄触觉反馈 | MOD-HAPTICS | `src/haptics/haptics.cpp` (SDL Rumble) | ✅ 实现 |
| REQ-112 | RGB 灯光同步 | MOD-HAPTICS | `src/haptics/haptics.cpp` (主色提取 + RgbCallback) | ✅ 实现 |
| REQ-113 | 直播互动 | MOD-SOCIAL | `src/social/social_bridge.cpp` (内置事件队列 + 文件监听) | ✅ 实现 |
| REQ-114 | 即时精彩回放 | MOD-REPLAY | `src/replay/replay.cpp` | ✅ 环形缓冲 + PPM 导出 |
| REQ-115 | 资源分析器 | MOD-RESOURCE | `src/resource/resource_analyzer.cpp` | ✅ tile/palette dump |
| REQ-116 | AI 实时超分（Mac PoC + 实时管线 + in-process） | MOD-VIDEO-AIUPSCALE | `src/video/ai_upscaler.cpp` + `src/video/ai_upscaler_ncnn.cpp` + `src/video/ai_upscale_service.cpp` + `tools/ai_upscale_demo.cpp` + `src/main.cpp --ai-upscale` | ✅ 子进程 + ncnn-vulkan inprocess（MoltenVK） |

## 测试矩阵 (Tests)

| 测试文件 | 类型 | 覆盖 | 状态 |
|----------|------|------|------|
| `tests/unit/cpu/cpu_test.cpp`           | Unit | REQ-001 / MOD-CPU       | ✅ 通过 |
| `tests/unit/ppu/ppu_test.cpp`           | Unit | REQ-002 / MOD-PPU       | ✅ 通过 |
| `tests/unit/apu/apu_test.cpp`           | Unit | REQ-003 / MOD-APU       | ✅ 通过 |
| `tests/unit/memory/memory_test.cpp`     | Unit | REQ-004 / MOD-MEMORY    | ✅ 通过 |
| `tests/unit/cartridge/cartridge_test.cpp` | Unit | REQ-005, REQ-008      | ✅ 通过 |
| `tests/unit/input/input_test.cpp`       | Unit | REQ-006                 | ✅ 通过 |
| `tests/unit/replay/replay_test.cpp`     | Unit | REQ-114                 | ✅ 通过 |
| `tests/unit/presets/presets_test.cpp`   | Unit | REQ-105, REQ-110        | ✅ 通过 |
| `tests/unit/savestate/savestate_test.cpp` | Unit | REQ-007              | ✅ 通过 |
| `tests/unit/social/social_test.cpp`     | Unit | REQ-113                 | ✅ 通过 |
| `tests/unit/audio/scene_test.cpp`       | Unit | REQ-107, REQ-109        | ✅ 通过 |
| `tests/unit/video/widescreen_test.cpp`  | Unit | REQ-103                 | ✅ 通过 |
| `tests/unit/overlay/overlay_test.cpp`   | Unit | REQ-010, REQ-012        | ✅ 通过 |
| `tests/unit/menu/menu_test.cpp`         | Unit | REQ-009, REQ-011, REQ-012 | ✅ 通过 |
| `tests/e2e/boot_test.cpp`               | E2E  | CPU+PPU+APU+Memory+Cart 集成 | ✅ 通过 |

## 硬件文档引用 (Hardware Docs)

| Hardware Doc | 引用方 |
|--------------|--------|
| `docs/hardware/cpu/instruction-set.md`     | REQ-001 / MOD-CPU |
| `docs/hardware/ppu/registers.md`           | REQ-002 / MOD-PPU |
| `docs/hardware/apu/channels.md`            | REQ-003 / MOD-APU |
| `docs/hardware/memory/cpu-bus.md`          | REQ-004 / MOD-MEMORY |
| `docs/hardware/cartridge/rom-format.md`    | REQ-005 / MOD-CARTRIDGE |
| `docs/hardware/cartridge/mappers/*.md`     | REQ-008 / MOD-CARTRIDGE |
| `docs/hardware/input/controller-protocol.md` | REQ-006 / MOD-INPUT |

## 维护说明

- 每次新增/完成一个阶段，增加或更新对应行。
- ✅ = 实现并测试通过；⚠️ = 接口/框架就绪，留待深度实现；⏸ = 已设计未实现。
