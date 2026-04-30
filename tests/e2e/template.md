# TEST-E2E-XXX: [端到端测试名称]

## 元数据 (Metadata)

- **ID**: TEST-E2E-XXX
- **关联需求 (Related Requirement)**: REQ-XXX
- **状态 (Status)**: Active | Disabled
- **最后运行 (Last Run)**: YYYY-MM-DD
- **结果 (Result)**: PASS | FAIL | SKIPPED

## 测试目的 (Test Purpose)

[What this end-to-end test verifies. 用中文描述。]

## 测试环境 (Test Environment)

- ROM: [测试用 ROM 文件]
- 模拟器配置: [配置说明]
- 预期帧数: [如 60 帧]

## 测试步骤 (Test Steps)

1. 启动模拟器并加载 ROM
2. 执行特定操作（如按键序列）
3. 观察结果

## 预期结果 (Expected Result)

[描述预期行为。]

## 实际结果 (Actual Result)

[执行后填写。]

## 测试脚本 (Test Script)

```bash
# Reference to test script: tests/e2e/scripts/xxx_e2e_test.sh
./fcemu rom.nes --test-script script.txt
```
