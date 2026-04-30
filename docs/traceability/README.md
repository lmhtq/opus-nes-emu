# 追溯 (Traceability)

本目录维护各阶段产出之间的追溯关系。

## 文件列表

| 文件 | 说明 |
|------|------|
| `matrix.md` | 追溯矩阵，记录 REQ → OVERVIEW → MOD → FEAT → src → tests 的完整链路 |

## 使用方式

- 每个阶段完成后，更新 `matrix.md`
- 可使用 `../../tools/generate-traceability.sh` 辅助生成

## 追溯链示例

```
docs/hardware/cpu/instruction-set.md
  → docs/specs/REQ-001.md
      → docs/overview/OVERVIEW-001.md
          → docs/module-design/MOD-CPU.md
              → docs/feature-design/FEAT-001.md
                  → src/cpu/cpu.cpp
                      → tests/unit/cpu/cpu_test.cpp
```

## 关联文档

- 需求规格：`../specs/`
- 概要设计：`../overview/`
- 模块设计：`../module-design/`
- 功能设计：`../feature-design/`
