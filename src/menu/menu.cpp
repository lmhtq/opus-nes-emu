#include "fcemu/menu.h"

#include <algorithm>

namespace fcemu {

// ---- MenuItem factories -----------------------------------------------------
MenuItem MenuItem::action(std::string l, std::function<void()> fn) {
    MenuItem it; it.kind = MenuItemKind::Action; it.label = std::move(l);
    it.on_activate = std::move(fn); return it;
}
MenuItem MenuItem::toggle(std::string l, std::function<bool()> g, std::function<void(bool)> s) {
    MenuItem it; it.kind = MenuItemKind::Toggle; it.label = std::move(l);
    it.get_bool = std::move(g); it.set_bool = std::move(s); return it;
}
MenuItem MenuItem::choice(std::string l, std::vector<std::string> opts,
                          std::function<int()> g, std::function<void(int)> s) {
    MenuItem it; it.kind = MenuItemKind::Choice; it.label = std::move(l);
    it.choices = std::move(opts); it.get_choice = std::move(g); it.set_choice = std::move(s);
    return it;
}
MenuItem MenuItem::keybind(std::string l, std::string action_id,
                           std::function<std::string()> g,
                           std::function<void(const std::string&)> s) {
    MenuItem it; it.kind = MenuItemKind::KeyBind; it.label = std::move(l);
    it.action_id = std::move(action_id);
    it.get_key_name = std::move(g); it.set_key_name = std::move(s); return it;
}
MenuItem MenuItem::submenu(std::string l, std::shared_ptr<Menu> sub) {
    MenuItem it; it.kind = MenuItemKind::Submenu; it.label = std::move(l);
    it.sub_menu = std::move(sub); return it;
}

// ---- MenuController ---------------------------------------------------------
void MenuController::open(std::shared_ptr<Menu> root) {
    stack_.clear();
    if (root) stack_.push_back({root, 0});
    capturing_ = false; capturing_item_ = -1;
}

void MenuController::close() {
    stack_.clear(); capturing_ = false; capturing_item_ = -1;
}

bool MenuController::on_key(MenuKey k) {
    if (stack_.empty()) return false;
    auto& fr = stack_.back();
    auto& it = fr.menu->items;
    if (it.empty()) {
        if (k == MenuKey::Back) { stack_.pop_back(); }
        return true;
    }
    int n = (int)it.size();
    switch (k) {
        case MenuKey::Up:   fr.sel = (fr.sel + n - 1) % n; break;
        case MenuKey::Down: fr.sel = (fr.sel + 1) % n; break;
        case MenuKey::Left:
        case MenuKey::Right: {
            MenuItem& cur = it[fr.sel];
            if (cur.kind == MenuItemKind::Toggle && cur.get_bool && cur.set_bool) {
                cur.set_bool(!cur.get_bool());
            } else if (cur.kind == MenuItemKind::Choice && cur.get_choice && cur.set_choice && !cur.choices.empty()) {
                int n2 = (int)cur.choices.size();
                int v = cur.get_choice();
                v = (k == MenuKey::Left) ? (v + n2 - 1) % n2 : (v + 1) % n2;
                cur.set_choice(v);
            }
            break;
        }
        case MenuKey::Activate: {
            MenuItem& cur = it[fr.sel];
            switch (cur.kind) {
                case MenuItemKind::Action:
                    if (cur.on_activate) cur.on_activate();
                    break;
                case MenuItemKind::Toggle:
                    if (cur.get_bool && cur.set_bool) cur.set_bool(!cur.get_bool());
                    break;
                case MenuItemKind::Choice:
                    if (cur.get_choice && cur.set_choice && !cur.choices.empty()) {
                        int n2 = (int)cur.choices.size();
                        cur.set_choice((cur.get_choice() + 1) % n2);
                    }
                    break;
                case MenuItemKind::KeyBind:
                    capturing_ = true; capturing_item_ = fr.sel;
                    break;
                case MenuItemKind::Submenu:
                    if (cur.sub_menu) stack_.push_back({cur.sub_menu, 0});
                    break;
            }
            break;
        }
        case MenuKey::Back:
            if (capturing_) { capturing_ = false; capturing_item_ = -1; break; }
            if (stack_.size() > 1) stack_.pop_back();
            else stack_.clear();
            break;
    }
    return true;
}

void MenuController::capture_key_name(const std::string& name) {
    if (!capturing_ || stack_.empty()) return;
    auto& it = stack_.back().menu->items;
    if (capturing_item_ >= 0 && capturing_item_ < (int)it.size()) {
        if (it[capturing_item_].kind == MenuItemKind::KeyBind && it[capturing_item_].set_key_name) {
            it[capturing_item_].set_key_name(name);
        }
    }
    capturing_ = false; capturing_item_ = -1;
}

// ---- Render -----------------------------------------------------------------
void MenuController::render(uint8_t* buf, int w, int h, Overlay& ov) const {
    if (stack_.empty()) return;

    // Dim the background so the menu reads clearly.
    Overlay::fill_rect(buf, w, h, 0, 0, w, h, RGBA{0, 0, 0, 130});

    const auto& fr = stack_.back();
    const auto& items = fr.menu->items;

    int line_h = 12;
    int n = std::max(1, (int)items.size());
    int panel_h = 28 + n * line_h + 18;
    int panel_w = std::min(w - 16, 240);
    int x = (w - panel_w) / 2;
    int y = (h - panel_h) / 2;

    Overlay::draw_panel(buf, w, h, x, y, panel_w, panel_h);

    // Title bar
    Overlay::fill_rect(buf, w, h, x + 1, y + 1, panel_w - 2, 12, RGBA{40, 60, 110, 230});
    Overlay::draw_text(buf, w, h, x + 6, y + 3, fr.menu->title, Overlay::White);

    int yy = y + 22;
    for (int i = 0; i < (int)items.size(); ++i) {
        const auto& it = items[i];
        bool sel = (i == fr.sel);
        if (sel) Overlay::fill_rect(buf, w, h, x + 2, yy - 1, panel_w - 4, 10,
                                    RGBA{60, 100, 180, 200});
        RGBA col = sel ? Overlay::White : Overlay::Gray;

        std::string left = (sel ? "> " : "  ") + it.label;
        Overlay::draw_text(buf, w, h, x + 6, yy, left, col);

        // Right side: state.
        std::string right;
        switch (it.kind) {
            case MenuItemKind::Toggle:
                right = (it.get_bool && it.get_bool()) ? "[ON]" : "[OFF]";
                break;
            case MenuItemKind::Choice:
                if (it.get_choice && !it.choices.empty()) {
                    int v = it.get_choice();
                    if (v >= 0 && v < (int)it.choices.size()) right = "< " + it.choices[v] + " >";
                }
                break;
            case MenuItemKind::KeyBind:
                if (capturing_ && i == capturing_item_) right = "[press key...]";
                else if (it.get_key_name) right = it.get_key_name();
                break;
            case MenuItemKind::Submenu: right = ">"; break;
            case MenuItemKind::Action:  break;
        }
        if (!right.empty()) {
            int rw = Overlay::text_width(right);
            Overlay::draw_text(buf, w, h, x + panel_w - 6 - rw, yy, right,
                               sel ? Overlay::Yellow : Overlay::Cyan);
        }
        yy += line_h;
    }

    // Footer hint.
    std::string hint = capturing_ ? "Press a key  ESC to cancel"
                                  : "Up/Down move  Enter select  Esc back";
    Overlay::draw_text(buf, w, h, x + 6, y + panel_h - 12, hint, Overlay::Gray);
}

} // namespace fcemu
