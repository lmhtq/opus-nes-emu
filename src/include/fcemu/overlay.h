// fcemu overlay — minimal in-frame UI primitives and toast queue.
// Renders 8x8 bitmap text and rectangles directly into an RGBA8888 buffer
// so it composites cleanly with the PPU output (and through video_enhancer).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fcemu {

struct RGBA { uint8_t r, g, b, a; };

class Overlay {
public:
    static const RGBA White;
    static const RGBA Black;
    static const RGBA Yellow;
    static const RGBA Red;
    static const RGBA Green;
    static const RGBA Cyan;
    static const RGBA Gray;
    static const RGBA DarkPanel;     // semi-opaque dark panel background
    static const RGBA AccentBlue;

    // Pixel ops on RGBA buffer of size w*h*4 (row-major, RGBA byte order).
    static void put_pixel(uint8_t* buf, int w, int h, int x, int y, RGBA c);
    static void blend_pixel(uint8_t* buf, int w, int h, int x, int y, RGBA c);
    static void fill_rect(uint8_t* buf, int w, int h, int x, int y, int rw, int rh, RGBA c);
    static void stroke_rect(uint8_t* buf, int w, int h, int x, int y, int rw, int rh, RGBA c);
    static void draw_panel(uint8_t* buf, int w, int h, int x, int y, int rw, int rh,
                           RGBA fill = DarkPanel, RGBA edge = AccentBlue);

    // Text drawing. scale=1 → 8x8, scale=2 → 16x16. Returns advance width in px.
    static int  draw_char(uint8_t* buf, int w, int h, int x, int y, char ch,
                          RGBA color, int scale = 1);
    static int  draw_text(uint8_t* buf, int w, int h, int x, int y,
                          const std::string& text, RGBA color, int scale = 1);
    static int  text_width(const std::string& text, int scale = 1);

    // Toast queue: ephemeral notifications stacked at the bottom.
    void post_toast(const std::string& text, float seconds = 2.0f, RGBA color = White);
    void update(float dt);
    void render_toasts(uint8_t* buf, int w, int h);

private:
    struct Toast { std::string text; float remaining; RGBA color; };
    std::vector<Toast> toasts_;
};

} // namespace fcemu
