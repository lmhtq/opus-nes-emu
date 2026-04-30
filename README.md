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

```bash
mkdir build && cd build
cmake ..
make
```

## 运行

```bash
./fcemu path/to/rom.nes
```

## 文档

完整文档索引：[docs/README.md](docs/README.md)

## 硬件参考

实现基于以下 FC/NES 硬件文档：
- 6502 CPU（Ricoh 2A03/2A07）
- PPU（Picture Processing Unit）
- APU（Audio Processing Unit）
- Mapper 0/1/2/3/4

详见 [docs/hardware/](docs/hardware/)。
