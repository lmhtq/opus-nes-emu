// fcemu menu — stack-based pause menu rendered through Overlay.
// Items: Action / Toggle / Choice / KeyBind / Submenu.  Designed to be
// driven by SDL keyboard events but does not depend on SDL itself; the UI
// layer translates keys to MenuKey before forwarding.
#pragma once

#include "fcemu/overlay.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace fcemu {

enum class MenuKey { Up, Down, Left, Right, Activate, Back };

class Menu;

enum class MenuItemKind { Action, Toggle, Choice, KeyBind, Submenu };

struct MenuItem {
    MenuItemKind kind;
    std::string label;

    // Action
    std::function<void()> on_activate;

    // Toggle
    std::function<bool()> get_bool;
    std::function<void(bool)> set_bool;

    // Choice
    std::vector<std::string> choices;
    std::function<int()> get_choice;
    std::function<void(int)> set_choice;

    // KeyBind: action_id is e.g. "p1.a"; current value is rendered, and when
    // activated the menu enters capture mode and the next key event is bound.
    std::string action_id;
    std::function<std::string()> get_key_name;
    std::function<void(const std::string&)> set_key_name;

    // Submenu
    std::shared_ptr<Menu> sub_menu;

    // Static factory helpers.
    static MenuItem action(std::string l, std::function<void()> fn);
    static MenuItem toggle(std::string l, std::function<bool()> g, std::function<void(bool)> s);
    static MenuItem choice(std::string l, std::vector<std::string> opts,
                           std::function<int()> g, std::function<void(int)> s);
    static MenuItem keybind(std::string l, std::string action_id,
                            std::function<std::string()> g,
                            std::function<void(const std::string&)> s);
    static MenuItem submenu(std::string l, std::shared_ptr<Menu> sub);
};

class Menu {
public:
    std::string title;
    std::vector<MenuItem> items;

    explicit Menu(std::string t = "Menu") : title(std::move(t)) {}
    void add(MenuItem it) { items.push_back(std::move(it)); }
    int  size() const { return (int)items.size(); }
};

class MenuController {
public:
    void open(std::shared_ptr<Menu> root);
    void close();
    bool is_open() const { return !stack_.empty(); }
    bool is_capturing_key() const { return capturing_; }

    // Returns true if event was consumed.
    bool on_key(MenuKey k);

    // When in keybind capture mode, call this with a human-readable key name
    // to commit the binding and exit capture mode.
    void capture_key_name(const std::string& name);

    void render(uint8_t* buf, int w, int h, Overlay& ov) const;

    // Read current selection (0-based) for testing.
    int selection() const { return stack_.empty() ? -1 : stack_.back().sel; }
    std::shared_ptr<Menu> top() const { return stack_.empty() ? nullptr : stack_.back().menu; }

private:
    struct Frame { std::shared_ptr<Menu> menu; int sel = 0; };
    std::vector<Frame> stack_;
    bool capturing_ = false;
    int  capturing_item_ = -1;
};

} // namespace fcemu
