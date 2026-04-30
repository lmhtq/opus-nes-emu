// input.cpp - Standard NES controller emulation. LSB-first shift protocol.
#include "fcemu/input.h"
#include <cstring>

namespace fcemu {

StandardController::StandardController() : shift_reg_(0), strobe_(false) {
    buttons_.fill(false);
    key_map_.fill(0);
}

void StandardController::strobe() {
    shift_reg_ = 0;
    for (size_t i = 0; i < buttons_.size(); ++i) {
        if (buttons_[i]) shift_reg_ |= (1u << i);
    }
}

uint8_t StandardController::read() {
    if (strobe_) {
        return (shift_reg_ & 1) ? 1 : 0;
    }
    uint8_t v = shift_reg_ & 1;
    shift_reg_ = (shift_reg_ >> 1) | 0x80; // After 8 reads, returns 1.
    return v;
}

void StandardController::set_button(Button btn, bool pressed) { buttons_[(size_t)btn] = pressed; }
bool StandardController::get_button(Button btn) const { return buttons_[(size_t)btn]; }

InputManager::InputManager() : strobe_active_(false) {}

void InputManager::set_controller(int port, std::unique_ptr<InputDevice> device) {
    if (port >= 0 && port < 2) controllers_[port] = std::move(device);
}
InputDevice* InputManager::get_controller(int port) {
    return (port >= 0 && port < 2) ? controllers_[port].get() : nullptr;
}

uint8_t InputManager::cpu_read(uint16_t addr) {
    int port = (addr & 1) ? 1 : 0;
    return controllers_[port] ? (controllers_[port]->read() | 0x40) : 0x40;
}

void InputManager::cpu_write(uint16_t addr, uint8_t val) {
    if ((addr & 0x0001) != 0) return; // only $4016 strobes
    bool ns = (val & 1) != 0;
    if (ns) {
        if (controllers_[0]) controllers_[0]->strobe();
        if (controllers_[1]) controllers_[1]->strobe();
    } else if (strobe_active_) {
        if (controllers_[0]) controllers_[0]->strobe();
        if (controllers_[1]) controllers_[1]->strobe();
    }
    // Set strobe state on every write so the controller re-loads while strobe is high.
    auto* sc0 = dynamic_cast<StandardController*>(controllers_[0].get());
    auto* sc1 = dynamic_cast<StandardController*>(controllers_[1].get());
    if (sc0) { sc0->strobe(); }
    if (sc1) { sc1->strobe(); }
    strobe_active_ = ns;
}

void InputManager::on_key_down(int key_code) {
    for (int port = 0; port < 2; ++port) {
        auto* sc = dynamic_cast<StandardController*>(controllers_[port].get());
        if (!sc) continue;
        for (size_t i = 0; i < (size_t)Button::COUNT; ++i) {
            if (sc->get_key_mapping((Button)i) == key_code) sc->set_button((Button)i, true);
        }
    }
}

void InputManager::on_key_up(int key_code) {
    for (int port = 0; port < 2; ++port) {
        auto* sc = dynamic_cast<StandardController*>(controllers_[port].get());
        if (!sc) continue;
        for (size_t i = 0; i < (size_t)Button::COUNT; ++i) {
            if (sc->get_key_mapping((Button)i) == key_code) sc->set_button((Button)i, false);
        }
    }
}

void InputManager::on_gamepad_button(int controller, Button btn, bool pressed) {
    if (controller >= 0 && controller < 2 && controllers_[controller]) {
        controllers_[controller]->set_button(btn, pressed);
    }
}

} // namespace fcemu
