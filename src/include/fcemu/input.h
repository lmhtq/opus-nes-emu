// include/fcemu/input.h
#pragma once

#include <cstdint>
#include <functional>
#include <array>
#include <memory>

namespace fcemu {

enum class Button { A, B, Select, Start, Up, Down, Left, Right, COUNT };
using ButtonState = std::array<bool, static_cast<size_t>(Button::COUNT)>;

class InputDevice {
public:
    virtual ~InputDevice() = default;
    virtual void strobe() = 0;
    virtual uint8_t read() = 0;
    virtual void set_button(Button btn, bool pressed) = 0;
    virtual bool get_button(Button btn) const = 0;
};

class StandardController : public InputDevice {
public:
    StandardController();
    void strobe() override;
    uint8_t read() override;
    void set_button(Button btn, bool pressed) override;
    bool get_button(Button btn) const override;

    // Keyboard remapping.
    void set_key_mapping(Button btn, int key_code) { key_map_[static_cast<size_t>(btn)] = key_code; }
    int  get_key_mapping(Button btn) const         { return key_map_[static_cast<size_t>(btn)]; }

    // Turbo (rapid-fire / 连发). When turbo for a button is true AND the button
    // is held, the effective pressed state alternates each turbo phase. Phase
    // advances via tick_turbo() (call once per frame). Default rate = 2 → 30Hz
    // at 60fps; rate=N means toggle every N frames.
    void set_turbo(Button btn, bool turbo) { turbo_[static_cast<size_t>(btn)] = turbo; }
    bool get_turbo(Button btn) const       { return turbo_[static_cast<size_t>(btn)]; }
    void set_turbo_rate(int frames)        { turbo_rate_ = (frames < 1) ? 1 : frames; }
    int  get_turbo_rate() const            { return turbo_rate_; }
    void tick_turbo();

private:
    ButtonState buttons_;
    uint8_t shift_reg_;
    bool strobe_;
    std::array<int,  static_cast<size_t>(Button::COUNT)> key_map_;
    std::array<bool, static_cast<size_t>(Button::COUNT)> turbo_{};
    int  turbo_rate_  = 2;
    int  turbo_count_ = 0;
    bool turbo_phase_ = false;
};

class InputManager {
public:
    InputManager();
    void set_controller(int port, std::unique_ptr<InputDevice> device);
    InputDevice* get_controller(int port);
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t val);
    void on_key_down(int key_code);
    void on_key_up(int key_code);
    void on_gamepad_button(int controller, Button btn, bool pressed);

private:
    std::array<std::unique_ptr<InputDevice>, 2> controllers_;
    bool strobe_active_;
};

} // namespace fcemu
