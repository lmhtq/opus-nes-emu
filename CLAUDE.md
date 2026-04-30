# CLAUDE.md - fcemu 开发指南

## 项目概述

fcemu 是一个 FC/NES 模拟器，主打**现代声光电体验**——让经典 FC 游戏在视觉、听觉、触觉上更具冲击力。

项目遵循 **8 阶段开发流程**：

| 阶段 | 中文名 | 目录 | 说明 |
|------|--------|------|------|
| 0 | 硬件参考 | `docs/hardware/` | FC/NES 硬件文档，实现依据 |
| 1 | 需求规格 | `docs/specs/` | 需求定义（REQ-XXX.md） |
| 2 | 概要设计 | `docs/overview/` | 系统级设计（OVERVIEW-XXX.md） |
| 3 | 模块设计 | `docs/module-design/` | 模块级设计（MOD-XXX.md） |
| 4 | 功能设计 | `docs/feature-design/` | 功能级设计（FEAT-XXX.md） |
| 5 | 功能实现 | `src/<module>/` | 代码实现 |
| 6 | 功能测试 | `tests/unit/` | 单元测试 |
| 7 | 端到端测试 | `tests/e2e/` | E2E 测试 |
| 8 | 集成测试 | `tests/integration/` | 集成测试 |

## 各阶段入口/出口标准

### Stage 1: 需求规格（docs/specs/）
- **入口**：无（起点）
- **出口**：需求文档通过评审，包含：功能描述、验收标准、关联硬件文档
- **模板**：`docs/specs/template.md`
- **ID 格式**：`REQ-XXX`（基础模拟器 001-020，特色功能 101-115）

### Stage 2: 概要设计（docs/overview/）
- **入口**：对应 REQ 已批准
- **出口**：概要设计文档完成，包含架构图、模块划分、技术栈
- **模板**：`docs/overview/template.md`
- **必须引用**：`docs/hardware/` 中对应的硬件文档

### Stage 3: 模块设计（docs/module-design/）
- **入口**：OVERVIEW 已批准
- **出口**：模块设计完成，包含接口设计、数据结构、依赖关系
- **模板**：`docs/module-design/template.md`
- **必须引用**：关联的 OVERVIEW 和硬件文档

### Stage 4: 功能设计（docs/feature-design/）
- **入口**：MOD 已批准
- **出口**：功能设计完成，包含接口定义、流程图、测试场景
- **模板**：`docs/feature-design/template.md`

### Stage 5: 功能实现（src/）
- **入口**：FEAT 已批准
- **出口**：代码实现完成，通过单元测试
- **规范**：C++17，4 空格缩进，snake_case 文件名，PascalCase 类名

### Stage 6-8: 测试（tests/）
- **入口**：实现完成
- **出口**：所有测试用例通过

## ID 命名规范

| 类型 | 格式 | 示例 |
|------|------|------|
| 需求 | REQ-XXX | REQ-001, REQ-101 |
| 概要设计 | OVERVIEW-XXX | OVERVIEW-001 |
| 模块设计 | MOD-XXX | MOD-CPU, MOD-PPU |
| 功能设计 | FEAT-XXX | FEAT-001 |
| 单元测试 | TEST-UNIT-XXX | TEST-UNIT-001 |
| E2E 测试 | TEST-E2E-XXX | TEST-E2E-001 |
| 集成测试 | TEST-INT-XXX | TEST-INT-001 |

## 追溯机制

- 每个文档模板包含"关联"字段（关联需求、关联模块、关联硬件文档）
- 追溯矩阵位于 `docs/traceability/matrix.md`
- 每完成一个阶段，自动更新追溯矩阵

## 特色功能（优先级 P1）

| 编号 | 功能名 |
|------|--------|
| REQ-101 | 画质增强（CRT/ HDR/抗锯齿） |
| REQ-102 | 高清纹理替换 |
| REQ-103 | 宽屏补丁（16:9） |
| REQ-104 | 视觉特效增强 |
| REQ-105 | 预制视觉包 |
| REQ-106 | 音质增强（立体声/3D/均衡器） |
| REQ-107 | 动态音效联动 |
| REQ-108 | 音频可视化 |
| REQ-109 | 音频替换系统（remix） |
| REQ-110 | 预制音频包 |
| REQ-111 | 手柄触觉反馈 |
| REQ-112 | RGB 灯光同步 |
| REQ-113 | 直播互动模式（含刷礼物触发道具） |
| REQ-114 | 即时精彩回放 |
| REQ-115 | 游戏资源分析器 |

## 自动化指引

Claude Code 可按以下顺序自动推进：
1. 读取 `docs/hardware/` 对应文档
2. 基于 `docs/specs/template.md` 创建 REQ
3. 基于 `docs/overview/template.md` 创建 OVERVIEW
4. 基于 `docs/module-design/template.md` 创建 MOD
5. 基于 `docs/feature-design/template.md` 创建 FEAT
6. 实现 `src/` 代码
7. 编写 `tests/` 测试
8. 更新 `docs/traceability/matrix.md`

## Git 工作流

```bash
# 功能分支
git checkout -b feature/REQ-XXX-short-desc

# 提交时引用需求
git commit -m "feat: implement REQ-XXX - short description"

# 每次阶段转换时更新追溯矩阵
```
