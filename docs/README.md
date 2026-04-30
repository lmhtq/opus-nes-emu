# fcemu 文档索引

本目录包含 fcemu 项目的所有文档，按 8 阶段开发流程组织。

## 快速导航

| 目录 | 说明 | 快速链接 |
|------|------|----------|
| `hardware/` | FC/NES 硬件参考手册 | [README](hardware/README.md) |
| `specs/` | 需求规格（Stage 1） | [README](specs/README.md), [Template](specs/template.md) |
| `overview/` | 概要设计（Stage 2） | [README](overview/README.md), [Template](overview/template.md) |
| `module-design/` | 模块设计（Stage 3） | [README](module-design/README.md), [Template](module-design/template.md) |
| `feature-design/` | 功能设计（Stage 4） | [README](feature-design/README.md), [Template](feature-design/template.md) |
| `traceability/` | 跨阶段追溯 | [Matrix](traceability/matrix.md) |

## 8 阶段开发流程

```
Stage 0: docs/hardware/    硬件参考（输入依据）
   ↓
Stage 1: docs/specs/       需求规格（输出：REQ-XXX.md）
   ↓
Stage 2: docs/overview/    概要设计（输出：OVERVIEW-XXX.md）
   ↓
Stage 3: docs/module-design/ 模块设计（输出：MOD-XXX.md）
   ↓
Stage 4: docs/feature-design/ 功能设计（输出：FEAT-XXX.md）
   ↓
Stage 5: src/              功能实现（输出：代码文件）
   ↓
Stage 6: tests/unit/       功能测试（输出：单元测试）
   ↓
Stage 7: tests/e2e/        端到端测试
   ↓
Stage 8: tests/integration/ 集成测试
```

## 各阶段准入/准出条件

### Stage 1: 需求规格
- **准入**：无（流程起点）
- **准出**：需求文档包含完整的功能描述、验收标准、关联硬件文档字段

### Stage 2: 概要设计
- **准入**：对应 REQ 已批准
- **准出**：包含架构图、模块划分表、技术栈说明、关联需求字段

### Stage 3: 模块设计
- **准入**：对应 OVERVIEW 已批准
- **准出**：包含接口设计（代码）、依赖关系表、数据结构、关联概要设计字段

### Stage 4: 功能设计
- **准入**：对应 MOD 已批准
- **准出**：包含接口定义、流程图、边界条件、测试场景、关联模块字段

### Stage 5: 功能实现
- **准入**：对应 FEAT 已批准
- **准出**：代码实现完成，通过编译，通过对应单元测试

### Stage 6: 功能测试
- **准入**：实现代码完成
- **准出**：所有单元测试用例通过，覆盖率 ≥ 80%

### Stage 7: 端到端测试
- **准入**：功能测试通过
- **准出**：E2E 测试场景全部通过

### Stage 8: 集成测试
- **准入**：E2E 测试通过
- **准出**：集成测试通过，准备发布

## 追溯矩阵

所有阶段的产出通过 `docs/traceability/matrix.md` 关联。

追溯链示例：
```
docs/hardware/cpu/instruction-set.md
  → docs/specs/REQ-001.md
    → docs/overview/OVERVIEW-001.md
      → docs/module-design/MOD-CPU.md
        → docs/feature-design/FEAT-001.md
          → src/cpu/cpu.cpp
            → tests/unit/cpu/cpu_test.cpp
```

## 硬件文档索引

| 文档 | 说明 |
|------|------|
| [CPU 指令集](hardware/cpu/instruction-set.md) | 6502 完整指令集 |
| [CPU 寻址模式](hardware/cpu/addressing-modes.md) | 13 种寻址模式 |
| [CPU 寄存器](hardware/cpu/registers.md) | A/X/Y/SP/PC/STATUS |
| [PPU 寄存器](hardware/ppu/registers.md) | $2000-$2007 |
| [PPU 渲染](hardware/ppu/rendering.md) | 背景/精灵渲染流程 |
| [PPU 精灵](hardware/ppu/sprites.md) | OAM、精灵评估 |
| [APU 寄存器](hardware/apu/registers.md) | $4000-$4017 |
| [APU 音频通道](hardware/apu/audio-channels.md) | 脉冲/三角/噪声/DMC |
| [内存映射](hardware/memory/memory-map.md) | $0000-$FFFF |
| [Mapper](hardware/cartridge/mappers.md) | Mapper 0/1/2/3/4 |
| [ROM 格式](hardware/cartridge/rom-format.md) | iNES 格式 |
| [控制器](hardware/input/controllers.md) | 标准手柄、光枪等 |

## 特色功能需求

| 编号 | 功能名 | 状态 |
|------|--------|------|
| REQ-101 | 画质增强 | 待设计 |
| REQ-102 | 高清纹理替换 | 待设计 |
| REQ-103 | 宽屏补丁 | 待设计 |
| REQ-104 | 视觉特效增强 | 待设计 |
| REQ-105 | 预制视觉包 | 待设计 |
| REQ-106 | 音质增强 | 待设计 |
| REQ-107 | 动态音效联动 | 待设计 |
| REQ-108 | 音频可视化 | 待设计 |
| REQ-109 | 音频替换系统 | 待设计 |
| REQ-110 | 预制音频包 | 待设计 |
| REQ-111 | 手柄触觉反馈 | 待设计 |
| REQ-112 | RGB 灯光同步 | 待设计 |
| REQ-113 | 直播互动模式 | 待设计 |
| REQ-114 | 即时精彩回放 | 待设计 |
| REQ-115 | 游戏资源分析器 | 待设计 |
