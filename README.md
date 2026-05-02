# fcemu - FC/NES Emulator with Modern Experience

fcemu 是一个 FC（Family Computer / NES）模拟器，主打**现代声光电体验**。

## 特色功能

### 视觉增强
- CRT 扫描线、曲率、光晕、HDR 色调映射
- 高清纹理替换（Tile 级）
- 宽屏补丁（16:9）
- 视觉特效增强（爆炸震动、动态模糊、粒子效果）
- 预制视觉包（自动匹配高清包）

### 听觉增强
- 立体声扩展、3D 空间音频、均衡器
- 动态音效联动（场景自适应）
- 音频可视化（波形/频谱）
- 音频替换系统（remix 音乐、音效替换）
- 预制音频包

### 触觉/灯光
- 手柄触觉反馈（震动、自适应扳机）
- RGB 灯光同步（根据游戏内容变化）

### 直播与回放
- 直播互动模式（弹幕/投票/刷礼物触发道具）
- 即时精彩回放（自动检测高光时刻）

### 资源工具
- 游戏资源分析器（自动提取可替换资源）

## 开发流程

本项目遵循 8 阶段开发流程，详见 [CLAUDE.md](CLAUDE.md)。

| 阶段 | 目录 |
|------|------|
| 硬件参考 | [docs/hardware/](docs/hardware/) |
| 需求规格 | [docs/specs/](docs/specs/) |
| 概要设计 | [docs/overview/](docs/overview/) |
| 模块设计 | [docs/module-design/](docs/module-design/) |
| 功能设计 | [docs/feature-design/](docs/feature-design/) |
| 功能实现 | [src/](src/) |
| 测试 | [tests/](tests/) |

## 构建

依赖：
- C++17 编译器
- CMake ≥ 3.16
- SDL2 (`brew install sdl2` / `apt install libsdl2-dev`)
- OpenGL（macOS / Linux / Windows 原生提供）

```bash
mkdir build && cd build
cmake ..
make
ctest          # 运行单元 + e2e 测试
```

## 运行

```bash
./fcemu path/to/rom.nes
```

按键映射（默认）：

| 键 | 功能 |
|----|------|
| Z / X | A / B |
| ↑↓←→ | 方向键 |
| Enter | Start |
| Right Shift | Select |
| **A / S** | **A 连发 / B 连发**（按住即自动连射 30Hz） |
| F1 / F2 | 存档 / 读档（写入 `<rom>.state`，载入需 mapper 一致） |
| F3 | 切换调试日志（每秒 stdout 打印 FPS / 当前场景 / 主色） |
| F4 | **打开主菜单**（亦可按 ESC） |
| F5 | Reset |
| **P** | **Photo Mode** — 当前帧 → SDXL Turbo 重绘 → `~/Desktop/fcemu-photo/`（需先启 daemon，见下） |
| Tab | 切换屏幕 HUD（FPS / 场景 / 宽屏 / RGB 主色 / 暂停指示） |
| ESC | 打开/关闭主菜单（菜单关闭时按下亦可，无菜单时再按一次退出） |

### Player 2 默认键位
| 键 | 功能 |
|----|------|
| I/J/K/L | ↑/←/↓/→ |
| G / H | A / B |
| V / B | Select / Start |
| T / Y | A 连发 / B 连发 |

### 手柄 (USB / Bluetooth, SDL_GameController)
插入即用，最多 2 名玩家，支持热插拔。

| 手柄按钮 | NES 映射 |
|----------|----------|
| 右脸键 (B / Circle) | A |
| 下脸键 (A / Cross)  | B |
| 上脸键 (Y / Triangle) | A 连发 |
| 左脸键 (X / Square)   | B 连发 |
| Back / Share | Select |
| Start / Options | Start |
| 十字键 / 左摇杆 | 方向键 |
| Guide / PS | Reset |

### 自定义键位 (`fcemu.ini`)
启动时读取，退出时保存。**推荐方式：在游戏中按 ESC 打开菜单 → Controls → Player 1/2 keys → 选择动作 → 按下任意键即完成绑定（无需重启、自动写回 ini）。** 如需手工编辑，键名使用 SDL 标准（如 `Z`, `Up`, `Return`, `Right Shift`），动作前缀 `p1.` / `p2.`：

```ini
key.p1.a=Z
key.p1.b=X
key.p1.a_turbo=A
key.p1.b_turbo=S
turbo.rate_frames=2

# 启用宽屏（320x240 输出，左右各填 32 像素）
video.widescreen=true
video.crt=true

# 直播互动：监听一个文本文件，一行一条事件
# 格式: gift <kind> [count] | cheer | shake [intensity] [ms] | chat <text> | vote <up|down>
social.watch_file=/tmp/fcemu_events.txt
```

## 现代化交互（GUI 菜单 / HUD / Toast）

不再需要手动编辑 `fcemu.ini`。运行游戏后：

- **ESC / F4** 打开主菜单（游戏自动暂停）。菜单导航：↑↓ 选择，← → 调整选项，Enter 确认/进入子菜单，Backspace/ESC 返回。
- **主菜单结构**：
  - Resume — 关闭菜单回到游戏
  - Save state / Load state / Reset — 同 F1/F2/F5
  - **Settings**
    - Video：CRT 滤镜开关、Widescreen 开关
    - Audio：场景固定（auto/action/boss/menu/calm）
    - Haptics：测试震动
    - HUD overlay 开关
  - **Controls** → Player 1 / Player 2 keys：列出全部 20 个动作（含连发），选中后按任意键即完成绑定，按 ESC 取消
  - Quit
- **Tab** 随时切换屏幕 HUD（FPS / 当前音频场景 / 宽屏状态 / RGB 主色 / 是否暂停）。
- **Toast 通知**：存档成功、场景切换、社交事件（cheer / gift / chat）、键位绑定完成等都会在右下角弹出 1.5–2 秒。
- 所有菜单内修改即时落盘到 `fcemu.ini`，下次启动自动恢复。
- **键位冲突自动处理**：在 Controls 子菜单里把某个键绑给一个动作时，如果该键已被占用，原占用动作会被自动解绑并以红色 toast 提示；启动时也会扫描 `fcemu.ini` 中的冲突并在 stdout 输出告警；菜单里还有 "Scan for key conflicts" 一键复查。

> 渲染零依赖：内置一份公有领域 8×8 位图字体（font8x8_basic by Daniel Hepper），叠加在 PPU 输出帧缓冲上，再走原有的视觉增强管线。



| 功能 | 说明 |
|------|------|
| 存档/读档 (REQ-007) | F1 写入 `<rom>.state`（含 CPU/PPU/APU/RAM/Mapper bank/PRG-RAM/CHR-RAM）；F2 读回 |
| 宽屏 (REQ-103) | 设 `video.widescreen=true` 后输出 320x240，两侧由边缘列复制并做亮度渐变 |
| 动态音效场景 (REQ-107) | `AudioEnhancer` 每 30 帧根据 RMS 自动切换 `action`/`boss`/`menu`/`calm`，调整 EQ + 立体声宽度 + 混响 |
| 音频 remix (REQ-109) | `AudioEnhancer::load_remix_track(name, raw_s16le_path)` + `trigger_remix_oneshot()` 把外部 PCM 一次性混入输出 |
| RGB 灯光 (REQ-112) | `HapticsManager` 每帧从画面提取主色（16x16 网格 + 饱和度加权），通过 `set_rgb_callback()` 输出给硬件 |
| 直播互动 (REQ-113) | `SocialBridge` 监听 ini 配置的文本文件，行内事件触发震动 / 屏幕闪 / 屏幕震 / 礼物连发 |
| 调试覆盖层 (REQ-010) | F3 切换；每秒打印 FPS、当前场景、宽屏状态、主色 |

## AI 实时超分（mac / Apple Silicon）

把 NES 输出的 256×240 实时超分到 1024×960，三档可选：

```bash
./build/fcemu --ai-mode=fast    rom.nes   # ~22 ms / 帧（默认 realcugan-denoise3x，60 fps）
./build/fcemu --ai-mode=medium  rom.nes   # ~58 ms / 帧（realesrgan-anime-6b，~17 fps）
./build/fcemu --ai-mode=quality rom.nes   # ~185 ms / 帧（realesrgan-x4plus，~5 fps，画质最强）
./build/fcemu --ai-upscale=<model-name> rom.nes   # 显式指定模型（覆盖 mode）
```

后端：CoreML（Metal/ANE）；模型 mlmodelc 已部署在 `models/`，10+ 模型可切换。
NVIDIA 后端待补。

## Photo Mode（照片级重绘 - REQ-117）

游戏中按 **P**：当前帧经 SDXL Turbo + ControlNet Tile 重绘成 1024×1024 摄影级
画面，保存到 `~/Desktop/fcemu-photo/`。完成 toast 弹屏右下角。

一次性安装（约 8.6 GiB 模型）：

```bash
cd tools/photo
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
./download_models.sh                       # 默认走 hf-mirror.com
```

启动 daemon（一次即可，常驻避免冷启动）：

```bash
nohup tools/photo/.venv/bin/python tools/photo/photo_repaint.py --daemon \
  > ~/Desktop/fcemu-photo/daemon.log 2>&1 &
```

完整说明见 [docs/photo-mode.md](docs/photo-mode.md)。M2 上单帧 ≈ 90 s。



| 功能 | 说明 |
|------|------|
| 存档/读档 (REQ-007) | F1 写入 `<rom>.state`（含 CPU/PPU/APU/RAM/Mapper bank/PRG-RAM/CHR-RAM）；F2 读回 |
| 宽屏 (REQ-103) | 设 `video.widescreen=true` 后输出 320x240，两侧由边缘列复制并做亮度渐变 |
| 动态音效场景 (REQ-107) | `AudioEnhancer` 每 30 帧根据 RMS 自动切换 `action`/`boss`/`menu`/`calm`，调整 EQ + 立体声宽度 + 混响 |
| 音频 remix (REQ-109) | `AudioEnhancer::load_remix_track(name, raw_s16le_path)` + `trigger_remix_oneshot()` 把外部 PCM 一次性混入输出 |
| RGB 灯光 (REQ-112) | `HapticsManager` 每帧从画面提取主色（16x16 网格 + 饱和度加权），通过 `set_rgb_callback()` 输出给硬件 |
| 直播互动 (REQ-113) | `SocialBridge` 监听 ini 配置的文本文件，行内事件触发震动 / 屏幕闪 / 屏幕震 / 礼物连发 |
| 调试覆盖层 (REQ-010) | F3 切换；每秒打印 FPS、当前场景、宽屏状态、主色 |

## 文档

完整文档索引：[docs/README.md](docs/README.md)

## 硬件参考

实现基于以下 FC/NES 硬件文档：
- 6502 CPU（Ricoh 2A03/2A07）
- PPU（Picture Processing Unit）
- APU（Audio Processing Unit）
- Mapper 0/1/2/3/4

详见 [docs/hardware/](docs/hardware/)。
