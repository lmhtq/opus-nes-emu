// include/fcemu/ai_upscaler.h
//
// AI 超分抽象接口（REQ-116 / MOD-VIDEO-AIUPSCALE）。
// PoC 阶段提供两种后端：
//   - NearestNeighborUpscaler：纯 CPU 最近邻，零依赖、可作基线/兜底/单测对照
//   - NcnnSubprocessUpscaler ：子进程调用 realesrgan-ncnn-vulkan
// 实时（在线）路径不在 PoC 范围内，详见 docs/specs/REQ-116-*.md。
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fcemu {

struct Frame {
    int id = 0;            // 调用方递增；批量必须保持顺序对应
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba; // 紧凑 RGBA8，size == width*height*4
};

struct UpscalerCaps {
    std::string backend_name;     // "nearest" | "ncnn-subprocess"
    std::string model_name;       // 例 "realesr-animevideov3"
    int fixed_scale = 0;          // 0 = 任意；其它表示只支持该倍率
    bool supports_batch = false;
    bool is_ai = false;           // false 表示传统插值
};

struct UpscalerConfig {
    int scale = 4;
    std::string model_name = "realesr-animevideov3";
    std::string model_dir;     // 探测：本字段 > $FCEMU_AIUP_MODEL_DIR > "./models"
    std::string binary_path;   // 探测：本字段 > $FCEMU_AIUP_BIN > PATH
    std::string tile_size = "0";
    std::string thread_spec = "2:4:2";
    bool keep_temp = false;
    int subprocess_timeout_sec = 60;
};

class IAiUpscaler {
public:
    virtual ~IAiUpscaler() = default;
    virtual bool init(const UpscalerConfig& cfg, std::string* err = nullptr) = 0;
    virtual UpscalerCaps caps() const = 0;
    virtual bool upscale(const Frame& in, Frame& out, std::string* err = nullptr) = 0;
    virtual bool upscale_batch(const std::vector<Frame>& in,
                               std::vector<Frame>& out,
                               std::string* err = nullptr) = 0;
};

std::unique_ptr<IAiUpscaler> make_nearest_upscaler();
std::unique_ptr<IAiUpscaler> make_ncnn_subprocess_upscaler();
std::unique_ptr<IAiUpscaler> make_ncnn_inprocess_upscaler();
std::unique_ptr<IAiUpscaler> make_coreml_upscaler();

} // namespace fcemu
