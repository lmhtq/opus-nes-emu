# TEST-INT-XXX: [集成测试名称]

## 元数据 (Metadata)

- **ID**: TEST-INT-XXX
- **关联模块 (Related Modules)**: MOD-XXX, MOD-YYY
- **关联需求 (Related Requirement)**: REQ-XXX
- **状态 (Status)**: Active | Disabled
- **最后运行 (Last Run)**: YYYY-MM-DD
- **结果 (Result)**: PASS | FAIL | SKIPPED

## 测试目的 (Test Purpose)

[What this integration test verifies. 用中文描述集成的模块间交互。]

## 涉及模块 (Modules Involved)

| 模块 | 职责 |
|------|------|
| MOD-CPU | 执行指令 |
| MOD-PPU | 渲染画面 |
| ... | ... |

## 测试场景 (Test Scenarios)

1. [场景 1：模块间正常交互]
2. [场景 2：异常处理]

## 预期结果 (Expected Result)

[描述预期行为。]

## 实际结果 (Actual Result)

[执行后填写。]

## 测试代码 (Test Code)

```cpp
// Reference: tests/integration/xxx_int_test.cpp
TEST(IntegrationTest, CpuPpuInteraction) {
    // ...
}
```
