# REQ-117: Photo Mode（照片级重绘）

## 元数据 (Metadata)

- **ID**: REQ-117
- **状态 (Status)**: Implemented
- **优先级 (Priority)**: P2
- **创建日期 (Created)**: 2026-05-02
- **最后更新 (Updated)**: 2026-05-02
- **作者 (Author)**: Copilot

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/ppu/rendering.md`（256×240 RGBA 帧来源）

## 关联需求 (Related Requirements)

- REQ-101 画质增强（C 档：离线照片级重绘）
- REQ-116 AI 实时超分（A/B 档；与本需求并列形成完整画质矩阵）

## 功能描述

游戏中按 **P** 键，把当前 PPU 帧（256×240 RGBA）经 SDXL Turbo + ControlNet
Tile 重绘成 1024×1024 摄影级 PNG，写入 `~/Desktop/fcemu-photo/`。

异步执行：玩家按 P 后立刻收到 `Photo: repainting...` toast 即可继续游戏，
完成后另一条 toast 提示文件名（成功绿色 / 失败红色）。

## 验收标准 (Acceptance Criteria)

| # | 条件 | 状态 |
|---|------|------|
| AC-1 | 按 P 后 100 ms 内出现 toast，不卡渲染线程 | ✅ |
| AC-2 | M2 / 24 GiB / MPS 上单帧端到端 ≤ 120 s | ✅ ~90 s |
| AC-3 | daemon 不在时自动 fallback 到一次性 spawn 模式 | ✅ |
| AC-4 | 输出 PNG 分辨率 ≥ 1024×1024，色彩无 NaN（fp16-fix VAE） | ✅ |
| AC-5 | 多次按 P 不丢请求（线程安全的 photo_queue） | ✅ |

## 实现 (Implementation)

| 组件 | 路径 |
|------|------|
| C++ 客户端（socket / spawn / 队列） | `src/main.cpp` `launch_photo_repaint()` / `g_photo_queue` |
| 热键 + 输入位 | `src/ui/ui.cpp` (SDLK_p), `src/include/fcemu/ui.h` (`InputSnapshot.photo`) |
| Python daemon + CLI | `tools/photo/photo_repaint.py` |
| 模型下载脚本 | `tools/photo/download_models.sh` |
| 依赖 | `tools/photo/requirements.txt` |
| 用户文档 | `docs/photo-mode.md` |

## 协议（C++ ↔ daemon）

Unix socket `/tmp/fcemu_photo.sock`（可经 `FCEMU_PHOTO_SOCKET` 覆盖）。
单行 JSON 请求 / 单行 JSON 回复：

```json
// req
{"frame_path":"/tmp/.../in.png","out_path":"/.../out.png","prompt":"...","steps":4,"strength":0.6,"side":1024,"use_controlnet":true}
// reply
{"ok":true,"out_path":"/.../out.png"}
```

## 性能 (Performance)

| 阶段 | 耗时 (M2 MPS) |
|------|---------------|
| pipeline load + warmup | ~63 s（一次性，daemon 启动时） |
| 单帧 no-CN 1024 4-step | 40-60 s |
| 单帧 + ControlNet Tile | 90-130 s |

## 依赖 (Dependencies)

- Python 3.11+
- PyTorch ≥ 2.1（自动 MPS）
- diffusers ≥ 0.30, transformers ≥ 4.40
- 模型权重 ~8.6 GiB（`tools/photo/download_models.sh` 拉取）

## 备注

- C 档非实时，定位"摄影模式"，不进入主渲染回路。
- NVIDIA 后端复用同一 daemon 协议，把 Python 端 device 切到 cuda 即可。
