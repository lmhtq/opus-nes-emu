// REQ-116 — AsyncUpscaleService 实现
#include "fcemu/ai_upscale_service.h"

#include <chrono>
#include <cstring>

namespace fcemu {

AsyncUpscaleService::AsyncUpscaleService() = default;

AsyncUpscaleService::~AsyncUpscaleService() {
    stop();
}

bool AsyncUpscaleService::start(std::unique_ptr<IAiUpscaler> upscaler,
                                const UpscalerConfig& cfg) {
    if (running_.load()) return false;
    if (!upscaler) return false;
    if (!upscaler->init(cfg)) return false;

    upscaler_ = std::move(upscaler);
    cfg_ = cfg;
    // 暂以 256x240 输入维度推断目标分辨率；首次 submit 后会按真实尺寸校正。
    target_w_ = 256 * cfg.scale;
    target_h_ = 240 * cfg.scale;
    out_buf_.assign((size_t)target_w_ * target_h_ * 4, 0);

    running_.store(true);
    worker_ = std::thread([this] { worker_loop(); });
    return true;
}

void AsyncUpscaleService::stop() {
    if (!running_.exchange(false)) return;
    in_cv_.notify_all();
    if (worker_.joinable()) worker_.join();
    upscaler_.reset();
}

void AsyncUpscaleService::submit(const uint8_t* rgba, int w, int h) {
    if (!running_.load()) return;
    {
        std::lock_guard<std::mutex> lk(in_mu_);
        if (in_pending_) {
            std::lock_guard<std::mutex> sl(stat_mu_);
            ++stats_.dropped;
        }
        in_w_ = w; in_h_ = h;
        size_t need = (size_t)w * h * 4;
        if (in_buf_.size() != need) in_buf_.resize(need);
        std::memcpy(in_buf_.data(), rgba, need);
        in_pending_ = true;
        std::lock_guard<std::mutex> sl(stat_mu_);
        ++stats_.submitted;
    }
    in_cv_.notify_one();
}

bool AsyncUpscaleService::try_get_latest(uint8_t* out_rgba, uint64_t* last_gen) {
    std::lock_guard<std::mutex> lk(out_mu_);
    if (!out_valid_) return false;
    if (last_gen && *last_gen == out_gen_) return false;
    std::memcpy(out_rgba, out_buf_.data(), out_buf_.size());
    if (last_gen) *last_gen = out_gen_;
    return true;
}

AsyncUpscaleStats AsyncUpscaleService::stats() const {
    std::lock_guard<std::mutex> lk(stat_mu_);
    return stats_;
}

void AsyncUpscaleService::worker_loop() {
    using clock = std::chrono::steady_clock;
    while (running_.load()) {
        // 等待 pending 输入
        std::vector<uint8_t> local;
        int w = 0, h = 0;
        {
            std::unique_lock<std::mutex> lk(in_mu_);
            in_cv_.wait_for(lk, std::chrono::milliseconds(100), [this]{
                return in_pending_ || !running_.load();
            });
            if (!running_.load()) break;
            if (!in_pending_) continue;
            local.swap(in_buf_);
            w = in_w_; h = in_h_;
            in_pending_ = false;
            in_buf_.reserve(local.size());
        }

        Frame in;
        in.id = 0;
        in.width = w;
        in.height = h;
        in.rgba.swap(local);

        std::vector<Frame> ins;
        ins.push_back(std::move(in));
        std::vector<Frame> outs;

        auto t0 = clock::now();
        bool ok = upscaler_->upscale_batch(ins, outs);
        auto t1 = clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        {
            std::lock_guard<std::mutex> sl(stat_mu_);
            if (ok && !outs.empty()) {
                ++stats_.processed;
                stats_.last_ms = ms;
                stats_.ema_ms = stats_.ema_ms == 0 ? ms : stats_.ema_ms * 0.8 + ms * 0.2;
            } else {
                ++stats_.failed;
            }
        }

        if (!ok || outs.empty()) continue;

        Frame& o = outs.front();
        std::lock_guard<std::mutex> ol(out_mu_);
        if (o.width != target_w_ || o.height != target_h_) {
            target_w_ = o.width;
            target_h_ = o.height;
        }
        size_t need = (size_t)o.width * o.height * 4;
        if (out_buf_.size() != need) out_buf_.resize(need);
        std::memcpy(out_buf_.data(), o.rgba.data(), need);
        ++out_gen_;
        out_valid_ = true;
    }
}

} // namespace fcemu
