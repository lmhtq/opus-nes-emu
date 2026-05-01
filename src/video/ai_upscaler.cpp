// src/video/ai_upscaler.cpp
//
// AI 超分两种后端实现（REQ-116 / FEAT-116）。
//
// NearestNeighborUpscaler  : 纯 CPU 最近邻 4×；零依赖；用作兜底/基线
// NcnnSubprocessUpscaler   : 调外部 realesrgan-ncnn-vulkan，目录批处理
//
// 仅在本编译单元定义 stb 实现宏，确保仅 link 一次。
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include "fcemu/ai_upscaler.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <signal.h>
#include <spawn.h>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace fs = std::filesystem;

namespace fcemu {

namespace {

bool ensure_alpha_opaque(std::vector<uint8_t>& rgba) {
    if (rgba.size() % 4 != 0) return false;
    for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 0xff;
    return true;
}

bool write_png(const std::string& path, const Frame& f, std::string* err) {
    Frame copy = f;
    if (!ensure_alpha_opaque(copy.rgba)) {
        if (err) *err = "invalid rgba buffer";
        return false;
    }
    int ok = stbi_write_png(path.c_str(), copy.width, copy.height, 4,
                            copy.rgba.data(), copy.width * 4);
    if (!ok && err) *err = "stbi_write_png failed: " + path;
    return ok != 0;
}

bool read_png(const std::string& path, Frame& f, std::string* err) {
    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) {
        if (err) *err = std::string("stbi_load failed: ") + path + " (" + stbi_failure_reason() + ")";
        return false;
    }
    f.width = w;
    f.height = h;
    f.rgba.assign(data, data + (size_t)w * h * 4);
    stbi_image_free(data);
    ensure_alpha_opaque(f.rgba);
    return true;
}

bool path_executable(const std::string& p) {
    struct stat st{};
    if (stat(p.c_str(), &st) != 0) return false;
    if (!S_ISREG(st.st_mode)) return false;
    return access(p.c_str(), X_OK) == 0;
}

std::string which(const std::string& name) {
    const char* p = std::getenv("PATH");
    if (!p) return {};
    std::stringstream ss(p);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (dir.empty()) continue;
        std::string full = dir + "/" + name;
        if (path_executable(full)) return full;
    }
    return {};
}

std::string discover_binary(const UpscalerConfig& cfg) {
    if (!cfg.binary_path.empty() && path_executable(cfg.binary_path))
        return cfg.binary_path;
    if (const char* e = std::getenv("FCEMU_AIUP_BIN"))
        if (e[0] && path_executable(e)) return e;
    return which("realesrgan-ncnn-vulkan");
}

std::string discover_model_dir(const UpscalerConfig& cfg) {
    if (!cfg.model_dir.empty()) return cfg.model_dir;
    if (const char* e = std::getenv("FCEMU_AIUP_MODEL_DIR"))
        if (e[0]) return e;
    return "./models";
}

bool model_present(const std::string& dir, const std::string& name, int scale) {
    auto exists_pair = [&](const std::string& base) {
        return fs::exists(fs::path(dir) / (base + ".bin")) &&
               fs::exists(fs::path(dir) / (base + ".param"));
    };
    if (exists_pair(name)) return true;
    // realesr-animevideov3 在磁盘上分多档：name-x{2,3,4}
    if (scale > 0 && exists_pair(name + "-x" + std::to_string(scale))) return true;
    return false;
}

std::string make_tmp_dir(const std::string& prefix) {
    std::string tmpl = (fs::temp_directory_path() / (prefix + "_XXXXXX")).string();
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');
    char* r = mkdtemp(buf.data());
    return r ? std::string(r) : std::string();
}

std::string frame_name(int idx) {
    char b[32];
    std::snprintf(b, sizeof(b), "frame_%06d.png", idx);
    return b;
}

// ------------------- NearestNeighborUpscaler -------------------
class NearestNeighborUpscaler : public IAiUpscaler {
public:
    bool init(const UpscalerConfig& cfg, std::string* err) override {
        cfg_ = cfg;
        if (cfg_.scale < 1) {
            if (err) *err = "scale must be >= 1";
            return false;
        }
        return true;
    }
    UpscalerCaps caps() const override {
        return {"nearest", "nearest-neighbor", 0, true, false};
    }
    bool upscale(const Frame& in, Frame& out, std::string* err) override {
        if (in.width <= 0 || in.height <= 0 ||
            (int)in.rgba.size() != in.width * in.height * 4) {
            if (err) *err = "invalid input frame";
            return false;
        }
        int s = cfg_.scale;
        out.id = in.id;
        out.width = in.width * s;
        out.height = in.height * s;
        out.rgba.assign((size_t)out.width * out.height * 4, 0xff);
        for (int y = 0; y < out.height; ++y) {
            int sy = y / s;
            const uint8_t* src = in.rgba.data() + (size_t)sy * in.width * 4;
            uint8_t* dst = out.rgba.data() + (size_t)y * out.width * 4;
            for (int x = 0; x < out.width; ++x) {
                int sx = x / s;
                std::memcpy(dst + x * 4, src + sx * 4, 4);
            }
        }
        return true;
    }
    bool upscale_batch(const std::vector<Frame>& in,
                       std::vector<Frame>& out,
                       std::string* err) override {
        out.clear();
        out.resize(in.size());
        for (size_t i = 0; i < in.size(); ++i)
            if (!upscale(in[i], out[i], err)) return false;
        return true;
    }
private:
    UpscalerConfig cfg_;
};

// ------------------- NcnnSubprocessUpscaler --------------------
class NcnnSubprocessUpscaler : public IAiUpscaler {
public:
    bool init(const UpscalerConfig& cfg, std::string* err) override {
        cfg_ = cfg;
        bin_ = discover_binary(cfg_);
        if (bin_.empty()) {
            if (err) {
                *err = "realesrgan-ncnn-vulkan binary not found "
                       "(checked: cfg.binary_path, $FCEMU_AIUP_BIN, $PATH). "
                       "Download from https://github.com/xinntao/Real-ESRGAN/releases "
                       "and set FCEMU_AIUP_BIN.";
            }
            return false;
        }
        model_dir_ = discover_model_dir(cfg_);
        if (!model_present(model_dir_, cfg_.model_name, cfg_.scale)) {
            if (err) {
                *err = "model files not found in '" + model_dir_ + "': " +
                       cfg_.model_name + ".{bin,param}. "
                       "Set --model-dir or $FCEMU_AIUP_MODEL_DIR.";
            }
            return false;
        }
        return true;
    }
    UpscalerCaps caps() const override {
        return {"ncnn-subprocess", cfg_.model_name, cfg_.scale, true, true};
    }
    bool upscale(const Frame& in, Frame& out, std::string* err) override {
        std::vector<Frame> ins{in}, outs;
        if (!upscale_batch(ins, outs, err)) return false;
        if (outs.size() != 1) {
            if (err) *err = "subprocess returned no output";
            return false;
        }
        out = std::move(outs[0]);
        return true;
    }
    bool upscale_batch(const std::vector<Frame>& in,
                       std::vector<Frame>& out,
                       std::string* err) override {
        out.clear();
        if (in.empty()) return true;

        std::string tmp = make_tmp_dir("fcemu_aiup");
        if (tmp.empty()) {
            if (err) *err = "mkdtemp failed";
            return false;
        }
        std::string in_dir  = tmp + "/in";
        std::string out_dir = tmp + "/out";
        fs::create_directories(in_dir);
        fs::create_directories(out_dir);

        // 写入所有输入 PNG
        for (size_t i = 0; i < in.size(); ++i) {
            std::string fn = in_dir + "/" + frame_name((int)i);
            if (!write_png(fn, in[i], err)) {
                cleanup(tmp);
                return false;
            }
        }

        // 拉起 realesrgan-ncnn-vulkan 子进程
        std::string scale_str = std::to_string(cfg_.scale);
        std::vector<std::string> args = {
            bin_,
            "-i", in_dir,
            "-o", out_dir,
            "-n", cfg_.model_name,
            "-s", scale_str,
            "-m", model_dir_,
            "-t", cfg_.tile_size,
            "-j", cfg_.thread_spec,
            "-f", "png",
        };
        std::vector<char*> argv;
        for (auto& s : args) argv.push_back(const_cast<char*>(s.c_str()));
        argv.push_back(nullptr);

        pid_t pid = 0;
        // 重定向 stderr → tmp/stderr.log（避免污染调用方），stdout 静默
        posix_spawn_file_actions_t fa;
        posix_spawn_file_actions_init(&fa);
        std::string err_log = tmp + "/stderr.log";
        posix_spawn_file_actions_addopen(&fa, 2, err_log.c_str(),
                                         O_WRONLY | O_CREAT | O_TRUNC, 0644);
        posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0);

        int rc = posix_spawn(&pid, bin_.c_str(), &fa, nullptr, argv.data(), environ);
        posix_spawn_file_actions_destroy(&fa);
        if (rc != 0) {
            if (err) *err = std::string("posix_spawn failed: ") + std::strerror(rc);
            cleanup(tmp);
            return false;
        }

        // 简单超时：每 100ms 轮询；超时则 SIGKILL
        int waited_ms = 0;
        int status = 0;
        while (true) {
            pid_t r = waitpid(pid, &status, WNOHANG);
            if (r == pid) break;
            if (r < 0) {
                if (err) *err = std::string("waitpid: ") + std::strerror(errno);
                cleanup(tmp);
                return false;
            }
            usleep(100 * 1000);
            waited_ms += 100;
            if (waited_ms >= cfg_.subprocess_timeout_sec * 1000) {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                if (err) *err = "subprocess timeout";
                cleanup(tmp);
                return false;
            }
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            if (err) {
                std::string s = "subprocess exit=" +
                                std::to_string(WEXITSTATUS(status));
                FILE* f = std::fopen(err_log.c_str(), "rb");
                if (f) {
                    char buf[2048];
                    size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
                    buf[n] = '\0';
                    s += " stderr_tail=";
                    s += buf;
                    std::fclose(f);
                }
                *err = s;
            }
            cleanup(tmp);
            return false;
        }

        // 按文件名顺序读取，要求与输入一一对应
        out.resize(in.size());
        for (size_t i = 0; i < in.size(); ++i) {
            std::string fn = out_dir + "/" + frame_name((int)i);
            Frame f;
            if (!read_png(fn, f, err)) {
                cleanup(tmp);
                return false;
            }
            f.id = in[i].id;
            out[i] = std::move(f);
        }

        cleanup(tmp);
        return true;
    }

private:
    void cleanup(const std::string& tmp) {
        if (cfg_.keep_temp) {
            std::fprintf(stderr, "[ai_upscaler] keeping tmp: %s\n", tmp.c_str());
            return;
        }
        std::error_code ec;
        fs::remove_all(tmp, ec);
    }
    UpscalerConfig cfg_;
    std::string bin_;
    std::string model_dir_;
};

} // namespace

std::unique_ptr<IAiUpscaler> make_nearest_upscaler() {
    return std::make_unique<NearestNeighborUpscaler>();
}
std::unique_ptr<IAiUpscaler> make_ncnn_subprocess_upscaler() {
    return std::make_unique<NcnnSubprocessUpscaler>();
}

} // namespace fcemu
