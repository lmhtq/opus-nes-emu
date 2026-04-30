# FEAT-005: 输入设备模拟#

## 元数据 (Metadata)

- **ID**: FEAT-005
- **关联模块 (Related Module)**: MOD-INPUT
- **关联需求 (Related Requirements)**: REQ-005
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30

## 功能描述 (Feature Description)

实现标准手柄输入模拟，支持选通机制和移位寄存器读取。

## 接口定义 (Interface Definition)

```cpp
class StandardController {
public:
    void strobe();
    uint8_t read();  // returns 1 bit per call
    void set_button(Button btn, bool pressed);
};
```

## 流程图 (Flow Chart)

```
[Write $4016 = 1]
    → [Strobe: load button states to shift register]
        → [Write $4016 = 0]
            → [For i = 0 to 7]:
                → [Read $4016: return bit (A,B,Select,Start,Up,Down,Left,Right)]
                    → [Shift register >> 1]
                        → [After 8 reads: return 1]
```

## 边界条件 (Edge Cases)

1. **第 9+ 次读取**：返回 1
2. **选通后未读取**：保持当前状态
3. **2 个手柄独立**：$4016 != $4017
4. **键盘映射**：SDL keycode → Button

## 测试场景 (Test Scenarios)

1. 选通正确：写入 1 加载，写入 0 开始移位
2. 8 次读取返回正确按钮状态
3. 按钮按下返回 1，松开返回 0
4. 第 9+ 次读取返回 1
5. 2 个手柄独立工作
6. 键盘映射正确
7. 真实手柄识别
8. 实时开关

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/input/controllers.md`

## 变更记录 (Change History)

- 2026-04-30: Initial version
