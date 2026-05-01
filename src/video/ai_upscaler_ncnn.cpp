// REQ-116 — in-process ncnn-vulkan AI 超分后端（macOS / MoltenVK）
//
// 与 NcnnSubprocessUpscaler 共用 IAiUpscaler 接口；区别：
//   - 不 fork 子进程，不写中间 PNG，直接 RGBA -> ncnn::Mat -> GPU 推理 -> RGBA
//   - 模型只 load 一次，GpuInstance/VkAllocator 复用
//   - 单帧延迟从 ~510 ms 降到 ~30-80 ms（M2 + animevideov3-x4，目标 30+ fps）
//
// 当前实现假设：
//   * 输入是 256×240 或 320×240（VideoEnhancer 输出），不分 tile
//   * 模型文件名规则：<model>-x<scale>.{param,bin}
//     例：realesr-animevideov3-x4.{param,bin}
//   * 输入 blob 名 "data"，输出 blob 名 "output"（Real-ESRGAN ncnn 模型约定）
#include "fcemu/ai_upscaler.h"

#include <net.h>
#include <gpu.h>
#include <mat.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>

namespace fcemu {

namespace {

namespace fs = std::filesystem;

std::string find_existing(const std::string& dir, const std::string& base, const std::string& ext) {
    fs::path p = fs::path(dir) / (base + ext);
    if (fs::exists(p)) return p.string();
    return {};
}

// 解析模型文件路径：先尝试 <name>，再尝试 <name>-x<scale>。
bool resolve_model_paths(const std::string& dir, const std::string& name, int scale,
                        std::string& param_path, std::string& bin_path,
                        std::string* err) {
    auto try_pair = [&](const std::string& base) {
        std::string p = find_existing(dir, base, ".param");
        std::string b = find_existing(dir, base, ".bin");
        if (!p.empty() && !b.empty()) {
            param_path = p;
            bin_path = b;
            return true;
        }
        return false;
    };
    if (try_pair(name)) return true;
    if (scale > 0 && try_pair(name + "-x" + std::to_string(scale))) return true;
    if (err) *err = "model files not found in '" + dir + "': " +
                    name + ".{bin,param} or " +
                    name + "-x" + std::to_string(scale) + ".{bin,param}";
    return false;
}

// 全局 Vulkan instance 引用计数（ncnn 要求 create/destroy 配对）。
int g_vk_refcnt = 0;
std::mutex g_vk_mu;

bool gpu_acquire(std::string* err) {
    std::lock_guard<std::mutex> lk(g_vk_mu);
    if (g_vk_refcnt == 0) {
        if (ncnn::create_gpu_instance() != 0) {
            if (err) *err = "ncnn::create_gpu_instance failed (MoltenVK 不可用?)";
            return false;
        }
    }
    ++g_vk_refcnt;
    return true;
}

void gpu_release() {
    std::lock_guard<std::mutex> lk(g_vk_mu);
    if (--g_vk_refcnt == 0) {
        ncnn::destroy_gpu_instance();
    }
}

class NcnnInProcessUpscaler : public IAiUpscaler {
public:
    ~NcnnInProcessUpscaler() override {
        net_.reset();
        if (vk_acquired_) gpu_release();
    }

    bool init(const UpscalerConfig& cfg, std::string* err) override {
        cfg_ = cfg;
        scale_ = cfg.scale;

        std::string model_dir = cfg.model_dir;
        if (model_dir.empty()) {
            const char* env = std::getenv("FCEMU_AIUP_MODEL_DIR");
            if (env) model_dir = env;
        }
        if (model_dir.empty()) model_dir = "./models";

        std::string param_path, bin_path;
        if (!resolve_model_paths(model_dir, cfg.model_name, cfg.scale,
                                 param_path, bin_path, err)) {
            return false;
        }

        if (!gpu_acquire(err)) return false;
        vk_acquired_ = true;

        gpu_count_ = ncnn::get_gpu_count();
        if (gpu_count_ <= 0) {
            if (err) *err = "no Vulkan-capable GPU detected (MoltenVK)";
            return false;
        }

        net_ = std::make_unique<ncnn::Net>();
        net_->opt.use_vulkan_compute = true;
        net_->opt.use_fp16_packed = true;
        net_->opt.use_fp16_storage = true;
        net_->opt.use_fp16_arithmetic = true;
        net_->opt.use_int8_storage = false;
        net_->set_vulkan_device(0);

        if (net_->load_param(param_path.c_str()) != 0) {
            if (err) *err = "load_param failed: " + param_path;
            return false;
        }
        if (net_->load_model(bin_path.c_str()) != 0) {
            if (err) *err = "load_model failed: " + bin_path;
            return false;
        }
        param_path_ = param_path;
        bin_path_ = bin_path;
        return true;
    }

    UpscalerCaps caps() const override {
        UpscalerCaps c;
        c.backend_name = "ncnn-inprocess";
        c.model_name = cfg_.model_name;
        c.fixed_scale = scale_;
        c.supports_batch = false;   // 串行 extract；外层批量是退化的循环
        c.is_ai = true;
        return c;
    }

    bool upscale(const Frame& in, Frame& out, std::string* err) override {
        if (!net_) {
            if (err) *err = "not initialized";
            return false;
        }
        if (in.width <= 0 || in.height <= 0 ||
            (int)in.rgba.size() != in.width * in.height * 4) {
            if (err) *err = "invalid input frame dimensions";
            return false;
        }

        // RGBA -> RGB（ncnn from_pixels 支持 PIXEL_RGBA2RGB）
        ncnn::Mat ncnn_in = ncnn::Mat::from_pixels(
            in.rgba.data(), ncnn::Mat::PIXEL_RGBA2RGB, in.width, in.height);
        const float norm[3] = {1.f / 255.f, 1.f / 255.f, 1.f / 255.f};
        ncnn_in.substract_mean_normalize(nullptr, norm);

        ncnn::Extractor ex = net_->create_extractor();
        if (ex.input("data", ncnn_in) != 0) {
            if (err) *err = "extractor.input(\"data\") failed";
            return false;
        }
        ncnn::Mat ncnn_out;
        if (ex.extract("output", ncnn_out) != 0) {
            if (err) *err = "extractor.extract(\"output\") failed";
            return false;
        }

        const float denorm[3] = {255.f, 255.f, 255.f};
        ncnn_out.substract_mean_normalize(nullptr, denorm);

        const int ow = ncnn_out.w;
        const int oh = ncnn_out.h;
        out.id = in.id;
        out.width = ow;
        out.height = oh;
        out.rgba.assign((size_t)ow * oh * 4, 255);

        // ncnn::Mat 是 planar (CHW)；转 packed RGBA。
        std::vector<unsigned char> rgb((size_t)ow * oh * 3);
        ncnn_out.to_pixels(rgb.data(), ncnn::Mat::PIXEL_RGB);
        const unsigned char* sp = rgb.data();
        unsigned char* dp = out.rgba.data();
        for (int i = 0; i < ow * oh; ++i) {
            dp[0] = sp[0];
            dp[1] = sp[1];
            dp[2] = sp[2];
            dp[3] = 255;
            sp += 3;
            dp += 4;
        }
        return true;
    }

    bool upscale_batch(const std::vector<Frame>& in,
                       std::vector<Frame>& out,
                       std::string* err) override {
        out.clear();
        out.resize(in.size());
        for (size_t i = 0; i < in.size(); ++i) {
            if (!upscale(in[i], out[i], err)) return false;
        }
        return true;
    }

private:
    UpscalerConfig cfg_;
    int scale_ = 4;
    int gpu_count_ = 0;
    bool vk_acquired_ = false;
    std::unique_ptr<ncnn::Net> net_;
    std::string param_path_, bin_path_;
};

} // namespace

std::unique_ptr<IAiUpscaler> make_ncnn_inprocess_upscaler() {
    return std::make_unique<NcnnInProcessUpscaler>();
}

} // namespace fcemu
