// Unit tests for fcemu Menu navigation and key-bind capture.
#include "fcemu/menu.h"

#include <cassert>
#include <cstdio>
#include <memory>

using namespace fcemu;

static void test_navigation_and_action() {
    auto m = std::make_shared<Menu>("Root");
    int counter = 0;
    m->add(MenuItem::action("A", [&]{ counter++; }));
    m->add(MenuItem::action("B", [&]{ counter += 10; }));
    m->add(MenuItem::action("C", [&]{ counter += 100; }));

    MenuController mc;
    mc.open(m);
    assert(mc.is_open());
    assert(mc.selection() == 0);

    mc.on_key(MenuKey::Down);
    assert(mc.selection() == 1);
    mc.on_key(MenuKey::Down);
    assert(mc.selection() == 2);
    mc.on_key(MenuKey::Down);
    assert(mc.selection() == 0); // wrap

    mc.on_key(MenuKey::Up);
    assert(mc.selection() == 2);

    mc.on_key(MenuKey::Activate);
    assert(counter == 100);
}

static void test_toggle_and_choice() {
    auto m = std::make_shared<Menu>("S");
    bool flag = false;
    int  pick = 1;
    m->add(MenuItem::toggle("Flag", [&]{ return flag; }, [&](bool v){ flag = v; }));
    m->add(MenuItem::choice("Pick", {"a","b","c"}, [&]{ return pick; }, [&](int v){ pick = v; }));

    MenuController mc; mc.open(m);
    mc.on_key(MenuKey::Activate); // toggle to true
    assert(flag);
    mc.on_key(MenuKey::Down);
    mc.on_key(MenuKey::Right);    // pick -> 2
    assert(pick == 2);
    mc.on_key(MenuKey::Left);
    assert(pick == 1);
}

static void test_keybind_capture() {
    auto m = std::make_shared<Menu>("Keys");
    std::string bound = "Z";
    m->add(MenuItem::keybind("p1.a", "p1.a",
        [&]{ return bound; },
        [&](const std::string& s){ bound = s; }));

    MenuController mc; mc.open(m);
    mc.on_key(MenuKey::Activate);
    assert(mc.is_capturing_key());
    mc.capture_key_name("Q");
    assert(!mc.is_capturing_key());
    assert(bound == "Q");
}

static void test_submenu_back() {
    auto child = std::make_shared<Menu>("Child");
    child->add(MenuItem::action("ok", [](){}));
    auto root = std::make_shared<Menu>("Root");
    root->add(MenuItem::submenu("Open", child));

    MenuController mc; mc.open(root);
    assert(mc.top()->title == "Root");
    mc.on_key(MenuKey::Activate);
    assert(mc.top()->title == "Child");
    mc.on_key(MenuKey::Back);
    assert(mc.top()->title == "Root");
    mc.on_key(MenuKey::Back);
    assert(!mc.is_open());
}

int main() {
    test_navigation_and_action();
    test_toggle_and_choice();
    test_keybind_capture();
    test_submenu_back();
    std::printf("[menu] OK\n");
    return 0;
}
