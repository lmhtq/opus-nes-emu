// main.cpp - fcemu entry point. Wires CPU+PPU+APU+Memory+Cartridge+Input+UI
// and the optional advanced experience modules (video / audio / haptics /
// replay / social).
#include "fcemu/cpu.h"
#include "fcemu/ppu.h"
#include "fcemu/apu.h"
#include "fcemu/memory.h"
#include "fcemu/cartridge.h"
#include "fcemu/input.h"
#include "fcemu/ui.h"
#include "fcemu/video_enhancer.h"
#include "fcemu/ai_upscaler.h"
#include "fcemu/ai_upscale_service.h"
#include "fcemu/audio_enhancer.h"
#include "fcemu/haptics.h"
#include "fcemu/replay.h"
#include "fcemu/presets.h"
#include "fcemu/social.h"
#include "fcemu/savestate.h"
#include "fcemu/menu.h"

#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "stb_image_write.h"

using namespace fcemu;

namespace {

constexpr int CPU_CYCLES_PER_FRAME = 29781;
constexpr uint32_t SAVESTATE_MAGIC   = 0x46434553u; // 'FCES'
constexpr uint32_t SAVESTATE_VERSION = 1;

// 拍照模式 toast 完成队列：daemon 后台线程把"保存成功/失败"消息塞入，
// 主循环每帧 drain 一次 post 给 overlay。
struct PhotoToast { std::string text; bool ok; };
std::mutex g_photo_mutex;
std::vector<PhotoToast> g_photo_queue;

void photo_post(const std::string& text, bool ok) {
    std::lock_guard<std::mutex> lk(g_photo_mutex);
    g_photo_queue.push_back({text, ok});
}

// Try connecting to the photo daemon on the given unix socket and send a JSON
// request. Returns true if the request was queued; the reply is awaited in a
// detached thread and result posted via photo_post().
bool photo_request_via_daemon(const std::string& sock_path,
                              const std::string& in_path,
                              const std::string& out_path,
                              const std::string& prompt) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (sock_path.size() >= sizeof(addr.sun_path)) { ::close(fd); return false; }
    std::strncpy(addr.sun_path, sock_path.c_str(), sizeof(addr.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        ::close(fd);
        return false;
    }

    std::ostringstream req;
    req << "{\"in\":\"" << in_path << "\",\"out\":\"" << out_path << "\"";
    if (!prompt.empty()) {
        // Crude JSON escape: backslash + double quote.
        std::string esc;
        esc.reserve(prompt.size());
        for (char c : prompt) {
            if (c == '\\' || c == '"') esc.push_back('\\');
            esc.push_back(c);
        }
        req << ",\"prompt\":\"" << esc << "\"";
    }
    req << "}\n";
    std::string body = req.str();
    ssize_t n = ::send(fd, body.data(), body.size(), 0);
    if (n != static_cast<ssize_t>(body.size())) {
        ::close(fd);
        return false;
    }

    std::thread([fd, in_path, out_path]() {
        std::string buf;
        char tmp[1024];
        while (true) {
            ssize_t r = ::recv(fd, tmp, sizeof(tmp), 0);
            if (r <= 0) break;
            buf.append(tmp, tmp + r);
            if (buf.find('\n') != std::string::npos) break;
            if (buf.size() > 16 * 1024) break;
        }
        ::close(fd);
        ::unlink(in_path.c_str());
        bool ok = buf.find("\"ok\": true") != std::string::npos
               || buf.find("\"ok\":true")  != std::string::npos;
        std::string base = out_path;
        auto slash = base.find_last_of('/');
        if (slash != std::string::npos) base = base.substr(slash + 1);
        if (ok) {
            photo_post(std::string("Photo saved: ") + base, true);
        } else {
            photo_post("Photo FAILED (see daemon log)", false);
        }
    }).detach();
    return true;
}

// 拍照模式：把 256x240 RGBA 帧写到 /tmp，spawn detached python 重绘脚本。
// 输出落在 ~/Desktop/fcemu-photo/<timestamp>.png；同时把原始 256x240
// 帧另存为 <rom>_<ts>_orig.png 以便和重绘结果做并排对比。
void launch_photo_repaint(const uint8_t* rgba, int w, int h,
                          const std::string& rom_basename) {
    char ts[64];
    std::time_t now = std::time(nullptr);
    std::strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", std::localtime(&now));
    std::string in_path  = std::string("/tmp/fcemu_photo_") + ts + ".png";
    const char* home = std::getenv("HOME");
    std::string out_dir  = std::string(home ? home : ".") + "/Desktop/fcemu-photo";
    std::string out_path  = out_dir + "/" + rom_basename + "_" + ts + ".png";
    std::string orig_path = out_dir + "/" + rom_basename + "_" + ts + "_orig.png";
    std::string mkdir_cmd = std::string("mkdir -p '") + out_dir + "'";
    (void)std::system(mkdir_cmd.c_str());
    if (!stbi_write_png(in_path.c_str(), w, h, 4, rgba, w * 4)) {
        std::fprintf(stderr, "[photo] failed to write %s\n", in_path.c_str());
        return;
    }
    // Persist the raw PPU frame alongside the repaint so users can compare.
    // Non-fatal: a failed orig dump shouldn't block the API call.
    if (!stbi_write_png(orig_path.c_str(), w, h, 4, rgba, w * 4)) {
        std::fprintf(stderr, "[photo] failed to write orig %s\n",
                     orig_path.c_str());
    } else {
        std::printf("[photo] orig -> %s\n", orig_path.c_str());
    }

    const char* sock_env = std::getenv("FCEMU_PHOTO_SOCKET");
    std::string sock_path = (sock_env && *sock_env) ? sock_env
                                                    : "/tmp/fcemu_photo.sock";
    const char* prompt_env = std::getenv("FCEMU_PHOTO_PROMPT");
    std::string prompt = (prompt_env && *prompt_env) ? prompt_env : "";

    if (photo_request_via_daemon(sock_path, in_path, out_path, prompt)) {
        std::printf("[photo] daemon -> %s\n", out_path.c_str());
        return;
    }

    const char* script = std::getenv("FCEMU_PHOTO_SCRIPT");
    const char* py     = std::getenv("FCEMU_PHOTO_PYTHON");
    if (!script || !*script) {
        std::fprintf(stderr,
            "[photo] no daemon at %s and FCEMU_PHOTO_SCRIPT not set —\n"
            "[photo] frame saved to %s. Start the daemon:\n"
            "[photo]   python photo_repaint.py --daemon\n",
            sock_path.c_str(), in_path.c_str());
        photo_post("Photo: no daemon (frame saved to /tmp)", false);
        return;
    }
    std::ostringstream cmd;
    cmd << "( " << (py && *py ? py : "python3")
        << " '" << script << "'"
        << " --in '" << in_path << "'"
        << " --out '" << out_path << "'";
    if (!prompt.empty()) cmd << " --prompt \"" << prompt << "\"";
    cmd << " >> '" << out_dir << "/photo.log' 2>&1 ; rm -f '" << in_path << "' ) &";
    std::printf("[photo] spawn -> %s\n", out_path.c_str());
    (void)std::system(cmd.str().c_str());
}

void map_buttons(StandardController& c, const InputSnapshot& s, bool p2) {
    if (!p2) {
        c.set_button(Button::A,      s.a);
        c.set_button(Button::B,      s.b);
        c.set_button(Button::Select, s.select);
        c.set_button(Button::Start,  s.start);
        c.set_button(Button::Up,     s.up);
        c.set_button(Button::Down,   s.down);
        c.set_button(Button::Left,   s.left);
        c.set_button(Button::Right,  s.right);
        c.set_turbo (Button::A, s.a_turbo);
        c.set_turbo (Button::B, s.b_turbo);
    } else {
        c.set_button(Button::A,      s.p2_a);
        c.set_button(Button::B,      s.p2_b);
        c.set_button(Button::Select, s.p2_select);
        c.set_button(Button::Start,  s.p2_start);
        c.set_button(Button::Up,     s.p2_up);
        c.set_button(Button::Down,   s.p2_down);
        c.set_button(Button::Left,   s.p2_left);
        c.set_button(Button::Right,  s.p2_right);
        c.set_turbo (Button::A, s.p2_a_turbo);
        c.set_turbo (Button::B, s.p2_b_turbo);
    }
}

bool save_state_to_file(const std::string& path,
                        const Cpu6502& cpu, const Memory& mem,
                        const Ppu& ppu, const Apu& apu, const Cartridge& cart) {
    std::vector<uint8_t> blob;
    Serializer s(blob);
    s.write(SAVESTATE_MAGIC);
    s.write(SAVESTATE_VERSION);
    int mapper_no = cart.mapper_number();
    s.write(mapper_no);
    cpu.serialize(s);
    mem.serialize(s);
    ppu.serialize(s);
    apu.serialize(s);
    cart.serialize(s);
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(blob.data()), blob.size());
    return (bool)f;
}

bool load_state_from_file(const std::string& path,
                          Cpu6502& cpu, Memory& mem,
                          Ppu& ppu, Apu& apu, Cartridge& cart) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    size_t sz = (size_t)f.tellg();
    f.seekg(0);
    std::vector<uint8_t> blob(sz);
    f.read(reinterpret_cast<char*>(blob.data()), sz);
    if (!f) return false;
    try {
        Deserializer d(blob.data(), blob.size());
        uint32_t magic, ver; int mapper_no;
        d.read(magic); d.read(ver); d.read(mapper_no);
        if (magic != SAVESTATE_MAGIC || ver != SAVESTATE_VERSION) return false;
        if (mapper_no != cart.mapper_number()) return false;
        cpu.deserialize(d);
        mem.deserialize(d);
        ppu.deserialize(d);
        apu.deserialize(d);
        cart.deserialize(d);
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "load_state: %s\n", e.what());
        return false;
    }
}

// Heuristic audio scene from APU rms.
const char* infer_scene(const std::vector<int16_t>& samples) {
    if (samples.empty()) return "calm";
    int64_t sumsq = 0;
    for (int16_t s : samples) sumsq += (int64_t)s * s;
    double rms = std::sqrt((double)sumsq / samples.size());
    if      (rms > 9000) return "boss";
    else if (rms > 5000) return "action";
    else if (rms > 1500) return "menu";
    else                 return "calm";
}

} // namespace

int main(int argc, char* argv[]) {
    std::printf("fcemu - FC/NES Emulator with Modern Experience\n");
    std::printf("Version 0.1.0\n");

    if (argc < 2) {
        std::printf("Usage: %s <rom_file.nes> [--ai-upscale[=<model>]] [--ai-scale=N] [--ai-mode={fast|medium|quality}]\n", argv[0]);
        return 1;
    }

    // ---- CLI flags --------------------------------------------------------
    std::string rom_path;
    bool ai_upscale_enabled = false;
    std::string ai_upscale_model;             // empty => derived from mode
    bool ai_model_explicit = false;
    std::string ai_upscale_mode = "fast";     // fast | medium | quality
    std::string ai_upscale_backend = "auto";  // auto | ncnn-inprocess | ncnn-subprocess
    int ai_upscale_scale = 4;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.rfind("--ai-upscale-backend=", 0) == 0) {
            ai_upscale_backend = a.substr(21);
        } else if (a.rfind("--ai-mode=", 0) == 0) {
            ai_upscale_mode = a.substr(10);
        } else if (a.rfind("--ai-upscale", 0) == 0) {
            ai_upscale_enabled = true;
            auto eq = a.find('=');
            if (eq != std::string::npos) {
                ai_upscale_model = a.substr(eq + 1);
                ai_model_explicit = true;
            }
        } else if (a.rfind("--ai-scale=", 0) == 0) {
            ai_upscale_scale = std::atoi(a.c_str() + 11);
            if (ai_upscale_scale != 2 && ai_upscale_scale != 3 && ai_upscale_scale != 4)
                ai_upscale_scale = 4;
        } else if (a[0] != '-' && rom_path.empty()) {
            rom_path = a;
        }
    }
    if (rom_path.empty()) rom_path = argv[1];

    // Resolve --ai-mode -> default model (only when --ai-upscale=<model> not given).
    // fast    : 60fps real-time      ~22ms  realcugan-denoise3x
    // medium  : balanced ~17fps       ~58ms  realesrgan-anime-6b
    // quality : best look, ~5fps     ~185ms  realesrgan-x4plus
    if (!ai_model_explicit) {
        if (ai_upscale_mode == "quality")      ai_upscale_model = "realesrgan-x4plus";
        else if (ai_upscale_mode == "medium")  ai_upscale_model = "realesrgan-anime-6b";
        else                                    ai_upscale_model = "realcugan-denoise3x";
    }

    Cartridge cart;
    if (!cart.load_rom(rom_path.c_str())) {
        std::fprintf(stderr, "Failed to load ROM: %s\n", rom_path.c_str());
        return 1;
    }
    std::printf("ROM: %s\n  mapper=%d  battery=%d  sha256=%s\n",
                cart.game_name().c_str(), cart.mapper_number(),
                cart.has_battery(), cart.sha256().c_str());

    Memory mem;
    Cpu6502 cpu;
    Ppu ppu;
    Apu apu;
    InputManager input;
    input.set_controller(0, std::make_unique<StandardController>());
    input.set_controller(1, std::make_unique<StandardController>());

    ppu.set_cartridge(&cart);
    ppu.set_nmi_callback([&]{ cpu.signal_nmi(); });
    apu.init(44100);
    apu.set_dmc_reader([&](uint16_t a){ return mem.read(a); });

    mem.set_ppu_callbacks(
        [&](uint16_t a){ return ppu.cpu_read(a); },
        [&](uint16_t a, uint8_t v){ ppu.cpu_write(a, v); });
    mem.set_apu_callbacks(
        [&](uint16_t a){ return apu.cpu_read(a); },
        [&](uint16_t a, uint8_t v){ apu.cpu_write(a, v); });
    mem.set_input_callbacks(
        [&](uint16_t a){ return input.cpu_read(a); },
        [&](uint16_t a, uint8_t v){ input.cpu_write(a, v); });
    mem.set_cart_callbacks(
        [&](uint16_t a){ return cart.cpu_read(a); },
        [&](uint16_t a, uint8_t v){ cart.cpu_write(a, v); });
    mem.set_oam_dma_callback([&](uint8_t page){
        uint8_t buf[256];
        for (int i = 0; i < 256; ++i) buf[i] = mem.read((page << 8) | i);
        ppu.oam_dma_write(buf);
        cpu.trigger_dma(page);
    });

    cpu.set_callbacks(
        [&](uint16_t a){ return mem.read(a); },
        [&](uint16_t a, uint8_t v){ mem.write(a, v); });

    UI ui;
    if (!ui.init(WindowConfig{})) {
        std::fprintf(stderr, "UI init failed\n");
        return 1;
    }
    ui.load_settings("fcemu.ini");
    ui.seed_default_bindings();
    ui.set_hud_visible(ui.get_setting("ui.hud") == "true");
    {
        auto cs = ui.find_key_conflicts();
        if (!cs.empty()) {
            std::printf("[ui] WARNING: %zu key binding conflict(s) detected:\n", cs.size());
            for (auto& p : cs) std::printf("  %s = %s\n", p.first.c_str(), p.second.c_str());
            std::printf("[ui]   open Menu -> Controls -> Scan for key conflicts to review.\n");
        }
    }
    {
        auto rate_str = ui.get_setting("turbo.rate_frames");
        if (!rate_str.empty()) {
            int r = std::atoi(rate_str.c_str());
            for (int p = 0; p < 2; ++p) {
                if (auto* sc = dynamic_cast<StandardController*>(input.get_controller(p)))
                    sc->set_turbo_rate(r > 0 ? r : 2);
            }
        }
    }
    ui.set_title(std::string("fcemu — ") + cart.game_name());

    // ---- Enhancement modules --------------------------------------------
    VideoEnhancer venh; venh.init();
    venh.enable_widescreen(ui.get_setting("video.widescreen") == "true");
    {
        CRTEffect crt{};
        crt.enabled = ui.get_setting("video.crt") != "false";
        crt.scanline_intensity = 0.35f;
        venh.set_crt_params(crt);
    }
    AudioEnhancer aenh; aenh.init(44100);
    HapticsManager haptics; haptics.init();

    // ---- AI 实时超分（REQ-116，可选，默认关闭） --------------------------
    std::unique_ptr<AsyncUpscaleService> aiup;
    std::vector<uint8_t> render_buf;        // target_w*target_h*4 渲染缓冲
    std::vector<uint8_t> ai_latest;         // 最近一次成功的 upscale 输出
    uint64_t ai_last_gen = 0;
    bool ai_have_latest = false;
    int ai_target_w = 0, ai_target_h = 0;
    if (ai_upscale_enabled) {
        UpscalerConfig cfg{};
        cfg.model_name = ai_upscale_model;
        cfg.scale = ai_upscale_scale;
        cfg.tile_size = "0";
        cfg.thread_spec = "2:4:2";
        std::unique_ptr<IAiUpscaler> up;
        std::string chosen;
#if FCEMU_HAVE_COREML
        if (ai_upscale_backend == "auto" || ai_upscale_backend == "coreml") {
            up = make_coreml_upscaler();
            chosen = "coreml";
        }
#endif
#if FCEMU_HAVE_NCNN
        if (!up && (ai_upscale_backend == "auto" || ai_upscale_backend == "ncnn-inprocess")) {
            up = make_ncnn_inprocess_upscaler();
            chosen = "ncnn-inprocess";
        }
#endif
        if (!up) {
            up = make_ncnn_subprocess_upscaler();
            chosen = "ncnn-subprocess";
        }
        aiup = std::make_unique<AsyncUpscaleService>();
        if (!aiup->start(std::move(up), cfg)) {
            std::fprintf(stderr, "[ai-upscale] failed to start backend=%s "
                         "(set FCEMU_AIUP_MODEL_DIR; subprocess 还需 FCEMU_AIUP_BIN)\n",
                         chosen.c_str());
            aiup.reset();
        } else {
            ai_target_w = aiup->target_w();
            ai_target_h = aiup->target_h();
            render_buf.assign((size_t)ai_target_w * ai_target_h * 4, 0);
            ai_latest.assign(render_buf.size(), 0);
            std::printf("[ai-upscale] enabled: backend=%s mode=%s model=%s scale=%d target=%dx%d\n",
                        chosen.c_str(),
                        ai_upscale_mode.c_str(),
                        ai_upscale_model.c_str(), ai_upscale_scale,
                        ai_target_w, ai_target_h);
            // Make the visible window match the upscaled texture, otherwise
            // SDL would shrink the high-res output back down to fit the
            // default 768x720 window — defeating the AI upscaling entirely.
            ui.set_window_size(ai_target_w, ai_target_h);
        }
    }

    haptics.set_rgb_callback([](RGBColor c){
        // Hardware integration is platform-specific; print only when
        // something downstream actually consumes this.
        (void)c;
    });
    ReplayManager replay; replay.init(); replay.start_recording();
    PresetManager presets; presets.init("presets");
    presets.find_matching_preset(cart.sha256(), PresetType::All);

    SocialBridge social;
    std::string watch_path = ui.get_setting("social.watch_file");
    social.init(watch_path);
    social.set_handler([&](const SocialEvent& ev){
        switch (ev.type) {
            case SocialEventType::Cheer:
                venh.trigger_hit_flash();
                haptics.trigger_vibration(VibrationIntensity::Medium, ev.duration_ms);
                ui.overlay().post_toast("Cheer!", 1.5f, Overlay::Yellow);
                break;
            case SocialEventType::Shake:
                venh.trigger_shake(ev.intensity / 100.0f, ev.duration_ms);
                haptics.trigger_vibration(VibrationIntensity::Strong, ev.duration_ms);
                ui.overlay().post_toast("Shake!", 1.2f, Overlay::Red);
                break;
            case SocialEventType::Gift:
                if (auto* sc = dynamic_cast<StandardController*>(input.get_controller(0))) {
                    sc->set_turbo(Button::A, true);
                }
                haptics.trigger_vibration(VibrationIntensity::Strong, ev.duration_ms);
                {
                    std::ostringstream os; os << "Gift " << ev.kind << " x" << ev.count;
                    ui.overlay().post_toast(os.str(), 2.0f, Overlay::Green);
                }
                break;
            case SocialEventType::Vote:
                ui.overlay().post_toast(std::string("Vote ") + ev.text, 1.5f, Overlay::Cyan);
                break;
            case SocialEventType::Chat:
                ui.overlay().post_toast(std::string("Chat: ") + ev.text, 2.5f, Overlay::White);
                break;
            default: break;
        }
    });

    // ---- Build the in-game menu (ESC / F4 to open) ----------------------
    auto root = std::make_shared<Menu>("fcemu");

    // Settings → Video
    auto m_video = std::make_shared<Menu>("Video");
    m_video->add(MenuItem::toggle("CRT scanlines",
        [&]{ return ui.get_setting("video.crt") != "false"; },
        [&](bool v){
            ui.set_setting("video.crt", v ? "true" : "false");
            CRTEffect c{}; c.enabled = v; c.scanline_intensity = 0.35f;
            venh.set_crt_params(c);
            ui.overlay().post_toast(v ? "CRT ON" : "CRT OFF", 1.0f);
        }));
    m_video->add(MenuItem::toggle("Widescreen 320x240",
        [&]{ return venh.widescreen_enabled(); },
        [&](bool v){
            venh.enable_widescreen(v);
            ui.set_setting("video.widescreen", v ? "true" : "false");
            ui.overlay().post_toast(v ? "Widescreen ON" : "Widescreen OFF", 1.0f);
        }));

    // Settings → Audio
    auto m_audio = std::make_shared<Menu>("Audio");
    static const std::vector<std::string> scene_opts =
        {"auto", "calm", "menu", "action", "boss", "victory"};
    m_audio->add(MenuItem::choice("Scene", scene_opts,
        [&]{
            std::string s = ui.get_setting("audio.scene");
            if (s.empty()) s = "auto";
            for (size_t i = 0; i < scene_opts.size(); ++i)
                if (scene_opts[i] == s) return (int)i;
            return 0;
        },
        [&](int v){
            std::string s = scene_opts[v];
            ui.set_setting("audio.scene", s);
            if (s != "auto") aenh.set_scene(s);
            ui.overlay().post_toast("Scene: " + s, 1.0f, Overlay::Cyan);
        }));

    // Settings → Haptics
    auto m_haptics = std::make_shared<Menu>("Haptics");
    m_haptics->add(MenuItem::action("Test rumble (strong 400ms)", [&]{
        haptics.trigger_vibration(VibrationIntensity::Strong, 400);
        ui.overlay().post_toast("Rumble!", 1.0f, Overlay::Red);
    }));

    // Controls → P1 / P2 keybinds
    auto build_keybind_menu = [&](const std::string& title, const std::string& prefix){
        auto m = std::make_shared<Menu>(title);
        for (auto& act : UI::action_ids()) {
            if (act.rfind(prefix, 0) != 0) continue;
            std::string label = act.substr(prefix.size());
            m->add(MenuItem::keybind(label, act,
                [&, act]{
                    auto v = ui.get_setting("key." + act);
                    return v.empty() ? std::string("?") : v;
                },
                [&, act](const std::string& name){
                    // Detect & auto-resolve conflicts: if another action already
                    // owns this key, unbind the other one and warn the user.
                    std::string other = ui.find_action_for_key(name, act);
                    if (!other.empty()) {
                        ui.clear_setting("key." + other);
                        ui.overlay().post_toast(
                            other + " unbound (was " + name + ")",
                            2.0f, Overlay::Red);
                    }
                    ui.set_setting("key." + act, name);
                    ui.overlay().post_toast(act + " -> " + name, 1.2f, Overlay::Yellow);
                }));
        }
        return m;
    };
    auto m_p1 = build_keybind_menu("Player 1 keys", "p1.");
    auto m_p2 = build_keybind_menu("Player 2 keys", "p2.");

    auto m_controls = std::make_shared<Menu>("Controls");
    m_controls->add(MenuItem::submenu("Player 1 keys", m_p1));
    m_controls->add(MenuItem::submenu("Player 2 keys", m_p2));
    m_controls->add(MenuItem::action("Reset all keys to default", [&]{
        ui.reset_default_bindings();
        ui.overlay().post_toast("All key bindings reset to defaults", 2.0f, Overlay::Yellow);
    }));
    m_controls->add(MenuItem::action("Scan for key conflicts", [&]{
        auto cs = ui.find_key_conflicts();
        if (cs.empty()) {
            ui.overlay().post_toast("No key conflicts.", 1.5f, Overlay::Green);
        } else {
            std::ostringstream os;
            os << cs.size() << " conflict(s): ";
            for (size_t i = 0; i < cs.size() && i < 4; ++i) {
                if (i) os << ", ";
                os << cs[i].first << "=" << cs[i].second;
            }
            ui.overlay().post_toast(os.str(), 3.0f, Overlay::Red);
        }
    }));

    // Settings root
    auto m_settings = std::make_shared<Menu>("Settings");
    m_settings->add(MenuItem::submenu("Video",   m_video));
    m_settings->add(MenuItem::submenu("Audio",   m_audio));
    m_settings->add(MenuItem::submenu("Haptics", m_haptics));
    m_settings->add(MenuItem::toggle("HUD overlay (Tab)",
        [&]{ return ui.hud_visible(); },
        [&](bool v){ ui.set_hud_visible(v); ui.set_setting("ui.hud", v ? "true" : "false"); }));

    // Root
    bool want_save = false, want_load = false, want_reset = false;
    root->add(MenuItem::action("Resume",      [&]{ ui.menu().close(); }));
    root->add(MenuItem::action("Save state (F1)", [&]{ want_save  = true; ui.menu().close(); }));
    root->add(MenuItem::action("Load state (F2)", [&]{ want_load  = true; ui.menu().close(); }));
    root->add(MenuItem::action("Reset (F5)",  [&]{ want_reset = true; ui.menu().close(); }));
    root->add(MenuItem::submenu("Settings",   m_settings));
    root->add(MenuItem::submenu("Controls",   m_controls));
    root->add(MenuItem::action("Quit",        [&]{ ui.menu().close(); /* set quit below */
        SDL_Event q; q.type = SDL_QUIT; SDL_PushEvent(&q);
    }));
    ui.set_root_menu(root);


    // Audio path: APU -> AudioEnhancer -> SDL queue.
    apu.set_sample_callback([&](const std::vector<int16_t>& s){
        std::vector<int16_t> processed;
        aenh.process_samples(s, processed);
        ui.push_audio(processed.data(), (int)processed.size());

        // Auto-scene heuristic — only when user hasn't pinned a scene.
        static int scene_div = 0;
        if (++scene_div >= 30) {
            scene_div = 0;
            std::string forced = ui.get_setting("audio.scene");
            if (forced.empty() || forced == "auto") {
                const char* scene = infer_scene(s);
                if (aenh.current_scene() != scene) {
                    aenh.set_scene(scene);
                    ui.overlay().post_toast(std::string("Scene: ") + scene, 1.0f, Overlay::Cyan);
                }
            }
        }
    });

    cpu.reset();
    ppu.reset();
    apu.reset();

    Uint64 perf_freq = SDL_GetPerformanceFrequency();
    Uint64 frame_target_ticks = perf_freq / 60;
    Uint64 next_tick = SDL_GetPerformanceCounter() + frame_target_ticks;

    std::string state_path = std::string(argv[1]) + ".state";
    int  frame_no = 0;
    Uint64 last_stat = SDL_GetPerformanceCounter();

    while (!ui.should_quit()) {
        ui.process_events();
        auto snap = ui.input_snapshot();

        // Menu actions can request these too via flags set in the menu lambdas.
        bool do_save  = snap.save_state || want_save;
        bool do_load  = snap.load_state || want_load;
        bool do_reset = snap.reset      || want_reset;
        want_save = want_load = want_reset = false;

        if (do_reset) {
            cpu.reset(); ppu.reset(); apu.reset();
            ui.overlay().post_toast("Reset", 1.0f, Overlay::Yellow);
        }
        if (do_save) {
            if (save_state_to_file(state_path, cpu, mem, ppu, apu, cart))
                ui.overlay().post_toast("Saved", 1.2f, Overlay::Green);
            else
                ui.overlay().post_toast("Save FAILED", 2.0f, Overlay::Red);
        }
        if (do_load) {
            if (load_state_from_file(state_path, cpu, mem, ppu, apu, cart))
                ui.overlay().post_toast("Loaded", 1.2f, Overlay::Green);
            else
                ui.overlay().post_toast("Load FAILED", 2.0f, Overlay::Red);
        }
        if (snap.photo) {
            // Use raw 256x240 PPU output (pre video-enhancer) — the python
            // pipeline does its own letterbox/upscale.
            std::string base = rom_path;
            auto slash = base.find_last_of('/');
            if (slash != std::string::npos) base = base.substr(slash + 1);
            auto dot = base.find_last_of('.');
            if (dot != std::string::npos) base = base.substr(0, dot);
            launch_photo_repaint(ppu.frame().pixels, 256, 240, base);
            ui.overlay().post_toast("Photo: repainting...", 2.0f, Overlay::Cyan);
        }
        // Drain photo daemon completion queue.
        {
            std::vector<PhotoToast> drained;
            {
                std::lock_guard<std::mutex> lk(g_photo_mutex);
                drained.swap(g_photo_queue);
            }
            for (const auto& t : drained) {
                ui.overlay().post_toast(t.text, 3.0f,
                    t.ok ? Overlay::Green : Overlay::Red);
            }
        }

        auto* sc1 = dynamic_cast<StandardController*>(input.get_controller(0));
        auto* sc2 = dynamic_cast<StandardController*>(input.get_controller(1));
        if (sc1) { map_buttons(*sc1, snap, false); sc1->tick_turbo(); }
        if (sc2) { map_buttons(*sc2, snap, true);  sc2->tick_turbo(); }

        social.tick();

        // Pause emulation while menu is up.
        if (!ui.menu_open()) {
            int budget = CPU_CYCLES_PER_FRAME;
            bool prev_irq = false;
            while (budget > 0) {
                bool now_irq = cart.irq_pending();
                if (now_irq && !prev_irq) cpu.signal_irq();
                prev_irq = now_irq;

                int c = cpu.step();
                ppu.step(c);
                apu.step(c);
                budget -= c;
                if (ppu.frame_complete()) break;
            }
        }

        // Post-process video and present (menu/HUD composited inside ui).
        int ow = 256, oh = 240;
        const uint8_t* out = venh.process(ppu.frame().pixels, &ow, &oh);
        if (!out) { out = ppu.frame().pixels; ow = 256; oh = 240; }

        // ---- AI 实时超分接入（REQ-116） --------------------------------
        // 始终以 ai_target 分辨率喂给 SDL，避免每帧重建 texture。
        const uint8_t* render_ptr = out;
        int render_w = ow, render_h = oh;
        if (aiup) {
            // 1) 把当前帧投递给后台 worker（可能被丢弃覆盖）
            aiup->submit(out, ow, oh);

            // 1.5) 校正：upscaler 输出尺寸可能随输入改变（例如 widescreen
            //      把 256×240 切到 320×240，输出从 1024 变成 1280）。当
            //      service 报告的 target_w/h 与我们当前缓冲不一致时，重新
            //      分配 ai_latest / render_buf 并调整窗口。否则会按旧 stride
            //      解新尺寸的数据，画面横向重复 + 扫描线压缩。
            int actual_tw = aiup->target_w();
            int actual_th = aiup->target_h();
            if (actual_tw > 0 && actual_th > 0 &&
                (actual_tw != ai_target_w || actual_th != ai_target_h)) {
                ai_target_w = actual_tw;
                ai_target_h = actual_th;
                size_t need = (size_t)ai_target_w * ai_target_h * 4;
                render_buf.assign(need, 0);
                ai_latest.assign(need, 0);
                ai_have_latest = false;
                ai_last_gen = 0;
                ui.set_window_size(ai_target_w, ai_target_h);
                std::printf("[ai-upscale] target resized to %dx%d (input %dx%d)\n",
                            ai_target_w, ai_target_h, ow, oh);
            }

            // 2) 拉取最新已完成的输出
            if (aiup->try_get_latest(ai_latest.data(), &ai_last_gen)) {
                ai_have_latest = true;
            }

            // 3) 渲染到固定 target 尺寸：有最新就用最新；否则把当前帧最近邻
            //    放大到 target，至少不让纹理尺寸来回变。
            if (ai_have_latest) {
                std::memcpy(render_buf.data(), ai_latest.data(), render_buf.size());
            } else {
                int sx = (ai_target_w + ow - 1) / ow;
                int sy = (ai_target_h + oh - 1) / oh;
                (void)sx; (void)sy;
                for (int y = 0; y < ai_target_h; ++y) {
                    int src_y = (int)((int64_t)y * oh / ai_target_h);
                    if (src_y >= oh) src_y = oh - 1;
                    const uint8_t* srow = out + (size_t)src_y * ow * 4;
                    uint8_t* drow = render_buf.data() + (size_t)y * ai_target_w * 4;
                    for (int x = 0; x < ai_target_w; ++x) {
                        int src_x = (int)((int64_t)x * ow / ai_target_w);
                        if (src_x >= ow) src_x = ow - 1;
                        const uint8_t* sp = srow + (size_t)src_x * 4;
                        drow[0] = sp[0]; drow[1] = sp[1];
                        drow[2] = sp[2]; drow[3] = 255;
                        drow += 4;
                    }
                }
            }
            render_ptr = render_buf.data();
            render_w = ai_target_w;
            render_h = ai_target_h;
        }

        // Refresh HUD lines.
        if (ui.hud_visible()) {
            std::vector<std::string> lines;
            lines.push_back(std::string("Scene ") + aenh.current_scene());
            lines.push_back(std::string("WS    ") + (venh.widescreen_enabled() ? "on" : "off"));
            char rgb[24];
            std::snprintf(rgb, sizeof(rgb), "RGB   %02X%02X%02X",
                          haptics.current_color().r,
                          haptics.current_color().g,
                          haptics.current_color().b);
            lines.push_back(rgb);
            if (aiup) {
                auto st = aiup->stats();
                char ai[64];
                std::snprintf(ai, sizeof(ai), "AIUP  %.0fms drop=%llu ok=%llu",
                              st.ema_ms,
                              (unsigned long long)st.dropped,
                              (unsigned long long)st.processed);
                lines.push_back(ai);
            }
            if (ui.menu_open()) lines.push_back("PAUSED");
            ui.set_hud_lines(std::move(lines));
        }

        ui.render_frame(render_ptr, render_w, render_h);
        haptics.update_from_frame(ppu.frame().pixels);

        if (replay.recording()) {
            FrameData fd{};
            fd.video.assign(ppu.frame().pixels, ppu.frame().pixels + 256*240*4);
            fd.timestamp = (uint64_t)frame_no * 16;
            (void)fd;
        }

        ++frame_no;
        if (aiup) {
            Uint64 now = SDL_GetPerformanceCounter();
            if (now - last_stat > perf_freq * 2) {
                auto st = aiup->stats();
                std::printf("[ai-upscale] frame=%d fps=%.1f submitted=%llu dropped=%llu "
                            "processed=%llu failed=%llu last=%.0fms ema=%.0fms\n",
                            frame_no, ui.fps(),
                            (unsigned long long)st.submitted,
                            (unsigned long long)st.dropped,
                            (unsigned long long)st.processed,
                            (unsigned long long)st.failed,
                            st.last_ms, st.ema_ms);
                std::fflush(stdout);
                last_stat = now;
            }
        }
        if (ui.debug_overlay()) {
            Uint64 now = SDL_GetPerformanceCounter();
            if (now - last_stat > perf_freq) {
                std::printf("[debug] frame=%d fps=%.1f scene=%s widescreen=%d rgb=#%02X%02X%02X\n",
                            frame_no, ui.fps(),
                            aenh.current_scene().c_str(),
                            (int)venh.widescreen_enabled(),
                            haptics.current_color().r,
                            haptics.current_color().g,
                            haptics.current_color().b);
                last_stat = now;
            }
        }

        Uint64 now = SDL_GetPerformanceCounter();
        if (now < next_tick) {
            Uint32 ms = (Uint32)((next_tick - now) * 1000 / perf_freq);
            if (ms > 0 && ms < 100) SDL_Delay(ms);
        }
        next_tick += frame_target_ticks;
    }

    // Persist battery RAM if the cartridge has it.
    if (cart.has_battery()) cart.save_battery_ram(std::string(argv[1]) + ".sav");

    ui.save_settings("fcemu.ini");
    ui.shutdown();
    return 0;
}
