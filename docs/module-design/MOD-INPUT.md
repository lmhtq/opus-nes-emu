# MOD-INPUT: 输入设备

## 元数据 (Metadata)

- **ID**: MOD-INPUT
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-005
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 功能职责 (Responsibilities)

实现 FC/NES 输入设备模拟。

核心职责：
1. 模拟标准手柄（A/B/Select/Start/Up/Down/Left/Right）
2. 实现手柄选通（Strobe）机制
3. 实现移位寄存器读取（每次读 $4016/$4017 返回 1 bit）
4. 支持 2 个手柄
5. 支持键盘映射到手柄按钮
6. 支持真实手柄输入（SDL2 Gamepad）
7. 可扩展支持其他设备（Zapper 光枪等）

## 接口设计 (Interface Design)

```cpp
// include/fcemu/input.h
#pragma once

#include <cstdint>
#include <functional>
#include <array>

namespace fcemu {

// 手柄按钮
enum class Button {
    A, B, Select, Start, Up, Down, Left, Right,
    COUNT
};

// 按钮状态（按下/松开）
using ButtonState = std::array<bool, static_cast<size_t>(Button::COUNT)>;

// 输入设备接口
class InputDevice {
public:
    virtual ~InputDevice() = default;
    virtual void strobe() = 0;              // 选通，加载按钮状态
    virtual uint8_t read() = 0;             // 读取 1 bit
    virtual void set_button(Button btn, bool pressed) = 0;
    virtual bool get_button(Button btn) const = 0;
};

// 标准手柄
class StandardController : public InputDevice {
public:
    StandardController();

    void strobe() override;
    uint8_t read() override;
    void set_button(Button btn, bool pressed) override;
    bool get_button(Button btn) const override;

    // 键盘映射
    void set_key_mapping(Button btn, int key_code);
    int get_key_mapping(Button btn) const;

private:
    ButtonState buttons_;
    uint8_t shift_reg_;   // 移位寄存器
    bool strobe_;          // 选通状态
};

// 输入管理器
class InputManager {
public:
    InputManager();

    // 注册设备
    void set_controller(int port, std::unique_ptr<InputDevice> device);
    InputDevice* get_controller(int port);

    // CPU 侧访问（来自 $4016/$4017）
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t val);

    // 键盘事件（来自 UI）
    void on_key_down(int key_code);
    void on_key_up(int key_code);

    // 真实手柄事件
    void on_gamepad_button(int controller, Button btn, bool pressed);

private:
    std::array<std::unique_ptr<InputDevice>, 2> controllers_;
    bool strobe_active_;
};

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-CPU | 输入寄存器被 CPU 通过 $4016/$4017 访问 |
| MOD-UI | UI 传递键盘/手柄事件到 InputManager |

## 数据结构 (Data Structures)

### 移位寄存器状态

```cpp
// 标准手柄移位寄存器（8-bit）
// bit 0: A, bit 1: B, bit 2: Select, bit 3: Start
// bit 4: Up, bit 5: Down, bit 6: Left, bit 7: Right
// 按钮按下 = 1, 松开 = 0
// 第 9 次及以后读取返回 1（移位寄存器空）
```

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/input/controllers.md` - 控制器参考

## 变更记录 (Change History)

- 2026-04-30: Initial version
