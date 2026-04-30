# 追溯矩阵 (Traceability Matrix)

本文档维护 fcemu 项目各阶段产出之间的追溯关系。

## 追溯链示例 (Example Traceability Chain)

```
docs/hardware/cpu/instruction-set.md
  → docs/specs/REQ-001.md
      → docs/overview/OVERVIEW-001.md
          → docs/module-design/MOD-CPU.md
              → docs/feature-design/FEAT-001.md
                  → src/cpu/cpu.cpp
                      → tests/unit/cpu/cpu_test.cpp
                          → tests/e2e/cpu_e2e_test.cpp
                              → tests/integration/cpu_int_test.cpp
```

## 需求到设计 (Requirements to Design)

| Requirement | Overview | Modules | Features |
|-------------|----------|---------|----------|
| REQ-001     | OVERVIEW-001 | MOD-CPU | FEAT-001 |
| REQ-002     | OVERVIEW-001 | MOD-PPU | FEAT-002 |
| REQ-003     | OVERVIEW-001 | MOD-APU | FEAT-003 |
| ...         | ...      | ...     | ...      |

## 设计到实现 (Design to Implementation)

| Module/Feature | Implementation Files | Unit Tests | Status |
|---------------|---------------------|------------|--------|
| MOD-CPU        | src/cpu/            | tests/unit/cpu/ | Planned |
| MOD-PPU        | src/ppu/            | tests/unit/ppu/ | Planned |
| FEAT-101       | src/video/           | tests/unit/video/ | Planned |
| ...           | ...                 | ...        | ...    |

## 硬件文档引用 (Hardware Docs Reference)

| Hardware Doc | Used by (Requirements) |
|--------------|----------------------|
| `docs/hardware/cpu/instruction-set.md` | REQ-001, REQ-002, ... |
| `docs/hardware/ppu/registers.md` | REQ-003, FEAT-004, ... |
| ... | ... |

## 完整追溯链 (Complete Traceability)

| ID | Type | Related Hardware | Related REQ | Related MOD | Related FEAT | Implementation | Tests | Status |
|----|------|-----------------|-------------|-------------|--------------|----------------|-------|--------|
| REQ-001 | Req | cpu/instruction-set | - | MOD-CPU | FEAT-001 | src/cpu/ | unit/cpu/ | Planned |
| OVERVIEW-001 | Overview | all | REQ-001~020 | MOD-* | FEAT-* | - | - | Planned |
| MOD-CPU | Mod | cpu/* | REQ-001 | - | FEAT-001 | src/cpu/ | unit/cpu/ | Planned |
| FEAT-001 | Feat | cpu/* | REQ-001 | MOD-CPU | - | src/cpu/ | unit/cpu/ | Planned |
| REQ-101 | Req | ppu/*, apu/* | - | MOD-VIDEO | FEAT-101 | src/video/ | unit/video/ | Planned |
| ... | ... | ... | ... | ... | ... | ... | ... | ... |

## 更新说明 (Update Notes)

- 每次完成一个阶段（如从 REQ 到 OVERVIEW），请更新本矩阵。
- 使用 `tools/generate-traceability.sh` 可自动生成部分内容。
- 本矩阵应随项目进展持续维护。
