# MOD-VIDEO-AIUPSCALE: AI 超分子模块

## 元数据 (Metadata)

- **ID**: MOD-VIDEO-AIUPSCALE
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-116
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-05-01
- **最后更新 (Updated)**: 2026-05-01

## 与 MOD-VIDEO 的关系

本模块是 `MOD-VIDEO`（画质增强）的一个**功能性兄弟**，独立设计的原因：

1. AI 超分不是单纯的"片元后处理"，而是改变输出分辨率的"重建"操作
2. 后端可插拔（CPU/Vulkan/Metal/CUDA/CoreML），生命周期可能跨进程
3. PoC 阶段使用子进程调用，与 `VideoEnhancer` 的同步在线路径明确隔离

最终管线（阶段 2 设计目标）：

```
PPU 256×240 RGBA
   │
   ▼
┌─────────────────────┐
│ MOD-VIDEO-AIUPSCALE │  ← 异步队列；缺省 nearest 兜底
│  (4× 重建)           │
└─────────┬───────────┘
          ▼
     1024×960 RGBA
          │
          ▼
┌─────────────────────┐
│  MOD-VIDEO          │  ← CRT / HDR / AA / widescreen / shake
└─────────┬───────────┘
          ▼
       Renderer
```

PoC 阶段（REQ-116）只跑 **离线/批处理** 路径，由 `fcemu_ai_upscale_demo` 工具
调用，不影响在线 `VideoEnhancer::process()`。

## 功能职责 (Responsibilities)

1. 抽象不同推理后端为统一 `IAiUpscaler` 接口
2. 提供 PoC 子进程后端 `NcnnSubprocessUpscaler`
3. 提供基线/兜底后端 `NearestNeighborUpscaler`
4. 单帧与批量两种 API
5. 能力描述（capabilities）：模型名、固定/可选倍率、是否支持批量、是否真 AI
6. 健壮的二进制/模型探测与错误反馈

## 接口设计 (Interface Design)

```cpp
// src/include/fcemu/ai_upscaler.h
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
    int scale = 4;                              // 期望倍率
    std::string model_name = "realesr-animevideov3";
    std::string model_dir;                      // 探测顺序：本字段 > $FCEMU_AIUP_MODEL_DIR > "./models"
    std::string binary_path;                    // 探测顺序：本字段 > $FCEMU_AIUP_BIN > PATH
    std::string tile_size = "0";                // ncnn -t；"0" 自动
    std::string thread_spec = "2:4:2";          // ncnn -j load:proc:save
    bool keep_temp = false;                     // 保留 tmpdir 便于调试
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

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-PPU | 提供 256×240 RGBA 源帧 |
| MOD-VIDEO | 阶段 2 起在管线中位于 AI 上采样之后 |
| third_party/stb | 头文件方式提供 PNG 编解码（PoC） |
| 外部二进制 | `realesrgan-ncnn-vulkan` + 模型文件（运行时探测，可缺失） |

## 数据结构 (Data Structures)

- `Frame` 为紧凑 RGBA8；alpha 在写盘前一律置 255，读盘后忽略 alpha
- 子进程批处理使用零填充帧名 `frame_000001.png`，保证字典序 == 时间序

## 错误模式 (Error Modes)

| 场景 | 行为 |
|------|------|
| 二进制找不到 | `init` 返回 false，`err` 写入诊断 |
| 模型找不到 | 同上 |
| 子进程退出码非 0 | `upscale_batch` 返回 false，保留 stderr 摘要 |
| 帧尺寸为 0 / 通道异常 | `upscale` 返回 false |
| 输出帧数与输入不一致 | 返回 false，保留 tmpdir |

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/ppu/rendering.md`

## 变更记录 (Change History)

- 2026-05-01: 初版（PoC 子进程后端）
- 2026-05-01: 增补 `AsyncUpscaleService`（`src/include/fcemu/ai_upscale_service.h` + `src/video/ai_upscale_service.cpp`）：单生产者/单消费者背压模型，主循环 submit 不阻塞，丢中间帧；fcemu 主程序 `--ai-upscale` CLI 启用；macOS 强制 SDL Metal renderer。
