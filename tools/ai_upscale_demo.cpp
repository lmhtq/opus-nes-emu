// tools/ai_upscale_demo.cpp
//
// AI 超分 PoC 工具（REQ-116 / FEAT-116）。
// 流程：boot ROM → 抓 N 帧 → AI 超分 → 写 side-by-side 蒙太奇 PNG → 报告耗时。
//
// 与 tools/headless.cpp 的 boot 逻辑一致；为避免大重构，PoC 阶段直接在此重复
// 一份精简的 boot 代码（详见 REQ-116 备注）。
// stb 实现宏在 src/video/ai_upscaler.cpp 内已定义，本文件只需声明 prototype。
#include "stb_image_write.h"

#include "fcemu/cpu.h"
#include "fcemu/ppu.h"
#include "fcemu/apu.h"
#include "fcemu/memory.h"
#include "fcemu/cartridge.h"
#include "fcemu/input.h"
#include "fcemu/ai_upscaler.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using clock_t_hr = std::chrono::high_resolution_clock;

static bool load_file(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    auto sz = f.tellg(); f.seekg(0);
    out.resize((size_t)sz);
    f.read(reinterpret_cast<char*>(out.data()), sz);
    return (bool)f;
}

struct Args {
    std::string rom;
    std::string model = "realesr-animevideov3";
    std::string model_dir;
    std::string bin_path;
    int frames = 120;
    int capture_from = 60;
    int capture_count = 1;
    int every = 1;
    std::string out_dir = "./aiup_out";
    std::string backend = "ncnn-subprocess";
    int scale = 4;
    bool keep_temp = false;
};

static void usage(const char* prog) {
    std::fprintf(stderr,
        "usage: %s <rom> [--model NAME] [--model-dir DIR] [--bin PATH]\n"
        "  [--frames N] [--capture-from N] [--capture-count N] [--every K]\n"
        "  [--out-dir DIR] [--backend nearest|ncnn-subprocess] [--scale N]\n"
        "  [--keep-temp]\n", prog);
}

static bool parse(int argc, char** argv, Args& a) {
    if (argc < 2) return false;
    a.rom = argv[1];
    for (int i = 2; i < argc; ++i) {
        std::string k = argv[i];
        auto need = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", name);
                return nullptr;
            }
            return argv[++i];
        };
        if (k == "--model")            { auto v = need("--model"); if (!v) return false; a.model = v; }
        else if (k == "--model-dir")   { auto v = need("--model-dir"); if (!v) return false; a.model_dir = v; }
        else if (k == "--bin")         { auto v = need("--bin"); if (!v) return false; a.bin_path = v; }
        else if (k == "--frames")      { auto v = need("--frames"); if (!v) return false; a.frames = std::atoi(v); }
        else if (k == "--capture-from"){ auto v = need("--capture-from"); if (!v) return false; a.capture_from = std::atoi(v); }
        else if (k == "--capture-count"){auto v = need("--capture-count"); if (!v) return false; a.capture_count = std::atoi(v); }
        else if (k == "--every")       { auto v = need("--every"); if (!v) return false; a.every = std::atoi(v); }
        else if (k == "--out-dir")     { auto v = need("--out-dir"); if (!v) return false; a.out_dir = v; }
        else if (k == "--backend")     { auto v = need("--backend"); if (!v) return false; a.backend = v; }
        else if (k == "--scale")       { auto v = need("--scale"); if (!v) return false; a.scale = std::atoi(v); }
        else if (k == "--keep-temp")   { a.keep_temp = true; }
        else { std::fprintf(stderr, "unknown arg: %s\n", k.c_str()); return false; }
    }
    return true;
}

// 抓取 PPU 帧到 Frame 列表
static bool boot_and_capture(const Args& a, std::vector<fcemu::Frame>& frames) {
    std::vector<uint8_t> rom;
    if (!load_file(a.rom, rom)) {
        std::fprintf(stderr, "cannot read rom: %s\n", a.rom.c_str());
        return false;
    }
    fcemu::Cartridge cart;
    if (!cart.load_rom_data(rom)) {
        std::fprintf(stderr, "invalid iNES file\n");
        return false;
    }
    fcemu::Memory mem;
    fcemu::Cpu6502 cpu;
    fcemu::Ppu ppu;
    fcemu::Apu apu; apu.init(44100);
    fcemu::InputManager input;
    input.set_controller(0, std::make_unique<fcemu::StandardController>());

    apu.set_dmc_reader([&](uint16_t x){ return mem.read(x); });
    ppu.set_cartridge(&cart);
    ppu.set_nmi_callback([&]{ cpu.signal_nmi(); });
    mem.set_ppu_callbacks([&](uint16_t x){return ppu.cpu_read(x);}, [&](uint16_t x, uint8_t v){ppu.cpu_write(x,v);});
    mem.set_apu_callbacks([&](uint16_t x){return apu.cpu_read(x);}, [&](uint16_t x, uint8_t v){apu.cpu_write(x,v);});
    mem.set_input_callbacks([&](uint16_t x){return input.cpu_read(x);}, [&](uint16_t x, uint8_t v){input.cpu_write(x,v);});
    mem.set_cart_callbacks([&](uint16_t x){return cart.cpu_read(x);}, [&](uint16_t x, uint8_t v){cart.cpu_write(x,v);});
    mem.set_oam_dma_callback([&](uint8_t p){
        uint8_t b[256]; for (int i = 0; i < 256; ++i) b[i] = mem.read((p << 8) | i);
        ppu.oam_dma_write(b); cpu.trigger_dma(p);
    });
    cpu.set_callbacks([&](uint16_t x){return mem.read(x);}, [&](uint16_t x, uint8_t v){mem.write(x,v);});
    cpu.reset(); ppu.reset(); apu.reset();

    int last_captured_frame = -1;
    int captured = 0;
    bool prev_irq = false;
    while (ppu.frame_count() < a.frames && captured < a.capture_count) {
        int c = cpu.step();
        ppu.step(c); apu.step(c);
        bool irq = cart.irq_pending();
        if (irq && !prev_irq) cpu.signal_irq();
        prev_irq = irq;

        int fc = ppu.frame_count();
        if (fc >= a.capture_from && fc != last_captured_frame &&
            ((fc - a.capture_from) % a.every == 0)) {
            const auto& fb = ppu.frame();
            fcemu::Frame f;
            f.id = fc;
            f.width = 256;
            f.height = 240;
            f.rgba.assign(fb.pixels, fb.pixels + 256 * 240 * 4);
            frames.push_back(std::move(f));
            ++captured;
            last_captured_frame = fc;
        }
    }
    if (frames.empty()) {
        std::fprintf(stderr, "no frames captured (frames=%d capture_from=%d)\n",
                     a.frames, a.capture_from);
        return false;
    }
    return true;
}

// 把两张 RGBA 帧水平拼接（左右），高度需相同；若不同则按较小者截。
static std::vector<uint8_t> hstack(const fcemu::Frame& l, const fcemu::Frame& r,
                                   int& out_w, int& out_h) {
    out_h = std::min(l.height, r.height);
    out_w = l.width + r.width;
    std::vector<uint8_t> buf((size_t)out_w * out_h * 4, 0xff);
    for (int y = 0; y < out_h; ++y) {
        std::memcpy(buf.data() + (size_t)y * out_w * 4,
                    l.rgba.data() + (size_t)y * l.width * 4,
                    (size_t)l.width * 4);
        std::memcpy(buf.data() + ((size_t)y * out_w + l.width) * 4,
                    r.rgba.data() + (size_t)y * r.width * 4,
                    (size_t)r.width * 4);
    }
    return buf;
}

int main(int argc, char** argv) {
    Args a;
    if (!parse(argc, argv, a)) { usage(argv[0]); return 1; }
    fs::create_directories(a.out_dir);
    std::string in_dir  = a.out_dir + "/in";
    std::string out_dir = a.out_dir + "/out";
    std::string mtg_dir = a.out_dir + "/montage";
    fs::create_directories(in_dir);
    fs::create_directories(out_dir);
    fs::create_directories(mtg_dir);

    std::printf("=== fcemu_ai_upscale_demo ===\n");
    std::printf("rom=%s backend=%s model=%s scale=%d\n",
                a.rom.c_str(), a.backend.c_str(), a.model.c_str(), a.scale);

    auto t0 = clock_t_hr::now();
    std::vector<fcemu::Frame> in_frames;
    if (!boot_and_capture(a, in_frames)) return 2;
    auto t1 = clock_t_hr::now();
    std::printf("captured %zu frame(s)\n", in_frames.size());

    // 1) 写入输入 PNG（便于离线检视）
    for (size_t i = 0; i < in_frames.size(); ++i) {
        char fn[64];
        std::snprintf(fn, sizeof(fn), "%s/in_%06d.png", in_dir.c_str(), in_frames[i].id);
        stbi_write_png(fn, in_frames[i].width, in_frames[i].height, 4,
                       in_frames[i].rgba.data(), in_frames[i].width * 4);
    }
    auto t2 = clock_t_hr::now();

    // 2) 构造 Upscaler
    std::unique_ptr<fcemu::IAiUpscaler> up;
    if (a.backend == "nearest") up = fcemu::make_nearest_upscaler();
    else if (a.backend == "ncnn-subprocess") up = fcemu::make_ncnn_subprocess_upscaler();
    else { std::fprintf(stderr, "unknown backend: %s\n", a.backend.c_str()); return 3; }

    fcemu::UpscalerConfig cfg;
    cfg.scale = a.scale;
    cfg.model_name = a.model;
    cfg.model_dir = a.model_dir;
    cfg.binary_path = a.bin_path;
    cfg.keep_temp = a.keep_temp;
    std::string err;
    if (!up->init(cfg, &err)) {
        std::fprintf(stderr, "upscaler init failed: %s\n", err.c_str());
        return 3;
    }
    auto caps = up->caps();
    std::printf("backend=%s model=%s ai=%d batch=%d\n",
                caps.backend_name.c_str(), caps.model_name.c_str(),
                caps.is_ai, caps.supports_batch);

    // 3) 推理
    auto t3 = clock_t_hr::now();
    std::vector<fcemu::Frame> out_frames;
    if (!up->upscale_batch(in_frames, out_frames, &err)) {
        std::fprintf(stderr, "upscale_batch failed: %s\n", err.c_str());
        return 4;
    }
    auto t4 = clock_t_hr::now();

    // 4) 写超分输出 PNG
    for (size_t i = 0; i < out_frames.size(); ++i) {
        char fn[64];
        std::snprintf(fn, sizeof(fn), "%s/out_%06d.png", out_dir.c_str(), out_frames[i].id);
        stbi_write_png(fn, out_frames[i].width, out_frames[i].height, 4,
                       out_frames[i].rgba.data(), out_frames[i].width * 4);
    }
    auto t5 = clock_t_hr::now();

    // 5) 蒙太奇：左 = nearest 4×（基线），右 = AI 输出
    auto nearest_up = fcemu::make_nearest_upscaler();
    fcemu::UpscalerConfig ncfg; ncfg.scale = a.scale;
    nearest_up->init(ncfg, nullptr);
    for (size_t i = 0; i < in_frames.size(); ++i) {
        fcemu::Frame baseline;
        nearest_up->upscale(in_frames[i], baseline);
        int w = 0, h = 0;
        auto buf = hstack(baseline, out_frames[i], w, h);
        char fn[64];
        std::snprintf(fn, sizeof(fn), "%s/montage_%06d.png", mtg_dir.c_str(), in_frames[i].id);
        stbi_write_png(fn, w, h, 4, buf.data(), w * 4);
        std::printf("wrote %s (%dx%d)\n", fn, w, h);
    }
    auto t6 = clock_t_hr::now();

    auto ms = [](auto a, auto b) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
    };
    std::printf("\n=== timing (ms) ===\n");
    std::printf("boot+capture : %lld\n", (long long)ms(t0, t1));
    std::printf("write_in_png : %lld\n", (long long)ms(t1, t2));
    std::printf("upscaler init: %lld\n", (long long)ms(t2, t3));
    std::printf("upscale_batch: %lld   (frames=%zu, %.1f ms/frame)\n",
                (long long)ms(t3, t4), in_frames.size(),
                in_frames.empty() ? 0.0 : (double)ms(t3, t4) / in_frames.size());
    std::printf("write_out_png: %lld\n", (long long)ms(t4, t5));
    std::printf("montage      : %lld\n", (long long)ms(t5, t6));
    std::printf("TOTAL        : %lld\n", (long long)ms(t0, t6));

    // 落 timing.json
    {
        std::ofstream j(a.out_dir + "/timing.json");
        j << "{\n"
          << "  \"backend\": \"" << caps.backend_name << "\",\n"
          << "  \"model\": \"" << caps.model_name << "\",\n"
          << "  \"frames\": " << in_frames.size() << ",\n"
          << "  \"boot_capture_ms\": " << ms(t0, t1) << ",\n"
          << "  \"write_in_ms\": "    << ms(t1, t2) << ",\n"
          << "  \"init_ms\": "        << ms(t2, t3) << ",\n"
          << "  \"upscale_ms\": "     << ms(t3, t4) << ",\n"
          << "  \"write_out_ms\": "   << ms(t4, t5) << ",\n"
          << "  \"montage_ms\": "     << ms(t5, t6) << ",\n"
          << "  \"total_ms\": "       << ms(t0, t6) << "\n"
          << "}\n";
    }
    return 0;
}
