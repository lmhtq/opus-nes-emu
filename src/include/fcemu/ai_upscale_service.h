// REQ-116 — 异步 AI 超分服务
//
// 设计要点：
// 1. 单生产者（emulator 主线程）/ 单消费者（worker 线程）
// 2. 输入槽 1 个：submit() 始终覆盖未消费的输入（背压策略：丢中间帧，保最新）
// 3. 输出槽 1 个：try_get_latest() 非阻塞读取，附带递增的 generation id
// 4. 当 worker 忙时主线程不阻塞；调用方需准备好回退渲染（最近邻放大）
//
// 适用于子进程后端（~7 fps），目标分辨率固定（init 时确定），主线程拿到的
// 输出 frame 始终是 target_w x target_h 的 RGBA8。
#pragma once

#include "fcemu/ai_upscaler.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace fcemu {

struct AsyncUpscaleStats {
    uint64_t submitted   = 0;  // submit() 调用总数
    uint64_t dropped     = 0;  // 因 worker 还在处理而被覆盖丢弃的输入帧
    uint64_t processed   = 0;  // worker 完成的帧数
    uint64_t failed      = 0;  // upscale_batch 返回 false 的次数
    double   last_ms     = 0;  // 最近一次成功 upscale 的耗时 (ms)
    double   ema_ms      = 0;  // 指数移动平均
};

class AsyncUpscaleService {
public:
    AsyncUpscaleService();
    ~AsyncUpscaleService();

    AsyncUpscaleService(const AsyncUpscaleService&) = delete;
    AsyncUpscaleService& operator=(const AsyncUpscaleService&) = delete;

    // 接管 upscaler 所有权；启动 worker 线程。
    // 失败时返回 false 并保留所有权释放给调用方？这里直接析构掉传入对象。
    bool start(std::unique_ptr<IAiUpscaler> upscaler, const UpscalerConfig& cfg);

    void stop();

    // 提交一帧待超分。会被后续 submit 覆盖（dropped++）。
    // 输入数据会被复制到内部缓冲；调用后 src 即可释放/复用。
    void submit(const uint8_t* rgba, int w, int h);

    // 获取最新已完成的输出帧。
    // out_rgba: 调用方负责的缓冲，长度需 >= target_w*target_h*4
    // last_gen: 调用方持有的上次读取的 generation；本次写入新的 generation。
    // 返回 true 表示拷贝了一帧新内容；false 表示自上次以来没有新输出。
    bool try_get_latest(uint8_t* out_rgba, uint64_t* last_gen);

    int target_w() const { return target_w_; }
    int target_h() const { return target_h_; }
    AsyncUpscaleStats stats() const;

private:
    void worker_loop();

    std::unique_ptr<IAiUpscaler> upscaler_;
    UpscalerConfig cfg_{};
    int target_w_ = 0;
    int target_h_ = 0;

    std::thread worker_;
    std::atomic<bool> running_{false};

    // input slot
    std::mutex in_mu_;
    std::condition_variable in_cv_;
    std::vector<uint8_t> in_buf_;
    int in_w_ = 0, in_h_ = 0;
    bool in_pending_ = false;

    // output slot
    std::mutex out_mu_;
    std::vector<uint8_t> out_buf_;
    uint64_t out_gen_ = 0;
    bool out_valid_ = false;

    // stats
    mutable std::mutex stat_mu_;
    AsyncUpscaleStats stats_{};
};

} // namespace fcemu
