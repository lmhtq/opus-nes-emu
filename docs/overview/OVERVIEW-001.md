# OVERVIEW-001: fcemu 系统概要设计

## 元数据 (Metadata)

- **ID**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-001~006, REQ-101~115
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 设计概述 (Design Overview)

fcemu 是一个 FC/NES 模拟器，使用 C++17 开发，主打**现代声光电体验**。系统采用分层架构，底层模拟 FC 硬件，上层叠加现代增强功能。

核心设计思路：
1. **核心模拟层**：精确模拟 FC/NES 硬件（CPU/PPU/APU/内存/卡带/输入）
2. **增强层**：在核心模拟之上叠加画质/音质/触觉等现代体验功能
3. **UI 层**：提供用户界面、窗口管理、设置等
4. **资源层**：预制包管理、资源分析、资源替换

## 架构图 (Architecture Diagram)

```
+---------------------------+
|          UI Layer         |
|  (Window, Menu, Settings)|
+--------+--------+---------+
         |        |         |
+--------v--------v---------+
|   Presets  | Live/Replay |  (Resource & Social)
|   (Visual/Audio packs)    |
+--------+--------------------+
         |
+--------v--------------------+
|       Enhancement Layer      |
|  Video(A) | Audio(M) | Haptics |
+--------+--------------------+
         |
+--------v--------------------+
|      Core Emulation Layer   |
| CPU | PPU | APU | Memory | Cart |
+--------+--------------------+
         |
+--------v--------------------+
|        Hardware Abstraction |
|    (Input, Audio Out, GPU) |
+---------------------------+
```

## 技术栈 (Tech Stack)

| 组件 | 技术选型 | 说明 |
|------|----------|------|
| 语言 | C++17 | 现代 C++，性能与表达力兼顾 |
| 构建系统 | CMake | 跨平台构建 |
| 渲染 | OpenGL 3.3+ | 跨平台图形 API |
| 窗口管理 | SDL2 / GLFW | 窗口创建、输入处理 |
| 音频输出 | SDL2 Audio / PortAudio | 跨平台音频输出 |
| 音频处理 | libsamplerate / custom | 重采样、音效处理 |
| 视频处理 | OpenGL Shaders / ImGui | CRT 效果、UI 叠加 |
| 测试框架 | Google Test | 单元测试、集成测试 |
| UI 框架 | Dear ImGui | 调试窗口、设置界面 |
| 压缩 | zlib | ROM 解压、存档压缩 |
| JSON | nlohmann/json | 配置文件、预制包 manifest |

## 模块划分 (Module Division)

| Module ID | Module Name | Description | Related REQ |
|-----------|-------------|-------------|-------------|
| MOD-CPU | CPU Emulator | 6502 CPU 模拟 | REQ-001 |
| MOD-PPU | PPU Emulator | PPU 渲染模拟 | REQ-002 |
| MOD-APU | APU Emulator | APU 音频模拟 | REQ-003 |
| MOD-MEMORY | Memory Map | 内存映射与卡带 Mapper | REQ-004 |
| MOD-CARTRIDGE | Cartridge | 卡带加载、Mapper 实现 | REQ-004 |
| MOD-INPUT | Input Device | 手柄输入模拟 | REQ-005 |
| MOD-UI | UI & Window | 窗口管理、菜单、设置 | REQ-006 |
| MOD-VIDEO | Video Enhancement | 画质增强（CRT/HD/抗锯齿） | REQ-101~105 |
| MOD-AUDIO | Audio Enhancement | 音质增强（立体声/3D/均衡器） | REQ-106~110 |
| MOD-HAPTICS | Haptics | 触觉反馈、RGB 灯光 | REQ-111~112 |
| MOD-SOCIAL | Social & Live | 直播互动、弹幕事件 | REQ-113 |
| MOD-REPLAY | Replay | 精彩回放、录制 | REQ-114 |
| MOD-RESOURCE | Resource Analyzer | 资源分析、提取、替换 | REQ-115 |
| MOD-PRESETS | Preset Manager | 预制包管理 | REQ-105, REQ-110 |

## 数据流 (Data Flow)

### 正常模拟流程

```
ROM File
  → Cartridge (load iNES header + PRG/CHR)
    → Memory Map (setup mapping)
      → CPU (fetch & execute instructions)
        → PPU (render scanlines)
          → APU (generate audio samples)
            → Video Enhancement (apply CRT/HD effects)
              → Audio Enhancement (apply 3D/equalizer)
                → Output (display + sound)
```

### 输入流程

```
User Input (keyboard/gamepad)
  → Input Module (map to NES buttons)
    → CPU (write to $4016/$4017)
      → Game logic reads buttons
```

### 资源替换流程

```
ROM Load
  → Resource Analyzer (extract resources)
    → Preset Manager (match preset pack by hash)
      → Video/Audio Modules (apply replacements)
        → Enhanced Output
```

## 关键设计决策 (Key Design Decisions)

### 1. 渲染架构
- PPU 渲染到内部纹理（256x240），然后由 Video Enhancement 层后处理
- 允许同时输出原始画面和增强画面（分屏对比）

### 2. 音频架构
- APU 生成原始采样（44100Hz），然后由 Audio Enhancement 层后处理
- 支持同时输出原始音频和增强音频

### 3. 预制包系统
- 基于 ROM SHA-256 哈希匹配
- 支持多个预制包共存，用户可切换
- manifest.json 定义替换规则

### 4. 跨平台
- 核心模拟层不依赖平台特定 API
- 平台相关代码封装在 Hardware Abstraction 层

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/cpu/instruction-set.md` - CPU 指令集
- `docs/hardware/ppu/registers.md` - PPU 寄存器
- `docs/hardware/apu/registers.md` - APU 寄存器
- `docs/hardware/memory/memory-map.md` - 内存映射
- `docs/hardware/cartridge/mappers.md` - Mapper 参考
- `docs/hardware/input/controllers.md` - 输入设备

## 变更记录 (Change History)

- 2026-04-30: Initial version
