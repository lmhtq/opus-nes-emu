// input.cpp - Input device emulation (partial)
#include "fcemu/input.h"
#include <cstring>

namespace fcemu {

StandardController::StandardController() : shift_reg_(0), strobe_(false) {
    std::memset(buttons_.data(), 0, sizeof(ButtonState));
    std::memset(key_map_.data(), 0, sizeof(decltype(key_map_)::value_type) * key_map_.size());
}

void StandardController::strobe() {
    strobe_ = true;
    // Load button state into shift register
    shift_reg_ = 0;
    for (size_t i = 0; i < static_cast<size_t>(Button::COUNT); ++i) {
        if (buttons_[i]) shift_reg_ |= (1u << i);
    }
}

uint8_t StandardController::read() {
    if (!strobe_) {
        if (shift_reg_ & 0x80) {
            uint8_t result = (shift_reg_ & 0x80) ? 1 : 0;
            shift_reg_ <<= 1;
            if (shift_reg_ == 0) shift_reg_ = 0xFF;  // After 8 reads, return 1
            return result;
        } else {
            return 0;
        }
    }
    return (shift_reg_ & 0x80) ? 1 : 0;
}

void StandardController::set_button(Button btn, bool pressed) {
    buttons_[static_cast<size_t>(btn)] = pressed;
}

bool StandardController::get_button(Button btn) const {
    return buttons_[static_cast<size_t>(btn)];
}

// InputManager
InputManager::InputManager() : strobe_active_(false) {
    for (auto& c : controllers_) c.reset();
}

void InputManager::set_controller(int port, std::unique_ptr<InputDevice> device) {
    if (port >= 0 && port < 2) controllers_[port] = std::move(device);
}

InputDevice* InputManager::get_controller(int port) {
    if (port >= 0 && port < 2) return controllers_[port].get();
    return nullptr;
}

uint8_t InputManager::cpu_read(uint16_t addr) {
    addr &= 0x0001;  // Mirror to $4016/$4017
    int port = (addr == 1) ? 1 : 0;
    if (controllers_[port]) return controllers_[port]->read();
    return 0;
}

void InputManager::cpu_write(uint16_t addr, uint8_t val) {
    if ((addr & 0x0001) == 0) {  // $4016
        bool new_strobe = (val & 1) != 0;
        if (strobe_active_ && !new_strobe) {
            // Strobe off: start shifting
        }
        strobe_active_ = new_strobe;
        if (strobe_active_) {
            if (controllers_[0]) controllers_[0]->strobe();
            if (controllers_[1]) controllers_[1]->strobe();
        }
    }
}

void InputManager::on_key_down(int key_code) {
    for (auto& c : controllers_) {
        if (!c) continue;
        // TODO: Map key_code to Button using key_map_
    }
}

void InputManager::on_key_up(int key_code) {
    // Similar to on_key_down
}

void InputManager::on_gamepad_button(int controller, Button btn, bool pressed) {
    if (controller >= 0 && controller < 2 && controllers_[controller]) {
        controllers_[controller]->set_button(btn, pressed);
    }
}

} // namespace fcemu
