# Photo Mode (REQ-117)

游戏里按 **P** 键，把当前 PPU 帧（256×240）经 SDXL Turbo + ControlNet Tile 重绘成
1024×1024 摄影级画面，保存到 `~/Desktop/fcemu-photo/`。

实现：fcemu 通过 Unix socket `/tmp/fcemu_photo.sock` 推送请求给一个常驻的
Python diffusion daemon。daemon 不在时自动 fallback 到一次性 spawn 模式。

## 一次性安装

仓库目录 `tools/photo/` 提供一切：

```
tools/photo/
├── photo_repaint.py     # daemon + CLI
├── download_models.sh   # 拉权重（默认走 hf-mirror.com）
└── requirements.txt     # python 依赖
```

### 1. 准备 Python 环境

```bash
cd tools/photo
python3 -m venv .venv
.venv/bin/pip install -U pip
.venv/bin/pip install -r requirements.txt
```

> 推荐 Python 3.11；Apple Silicon 自动用 MPS 后端。

### 2. 下载模型权重（约 8.6 GiB）

```bash
./download_models.sh
```

下载到：
- `~/sdxl-turbo-local/`   — stabilityai/sdxl-turbo (fp16，约 6.5 GiB)
- `~/sdxl-extras/vae-fp16-fix/`   — madebyollin/sdxl-vae-fp16-fix (~330 MiB)
- `~/sdxl-extras/cn-tile/`        — xinsir/controlnet-tile-sdxl-1.0 (~2.4 GiB)

走官方 huggingface.co 镜像：`HF_MIRROR=https://huggingface.co ./download_models.sh`
自定义存储位置：`SDXL_TURBO_DIR=/path/x SDXL_EXTRAS_DIR=/path/y ./download_models.sh`

## 启动

### 1. 起 daemon（需要时启动一次）

```bash
nohup tools/photo/.venv/bin/python tools/photo/photo_repaint.py --daemon \
  > ~/Desktop/fcemu-photo/daemon.log 2>&1 &
```

日志出现 `[photo-daemon] listening on /tmp/fcemu_photo.sock` 就绪。
首次包含 ~18 s 加载 + ~45 s 1×1 算子 warmup。

### 2. 启 fcemu，游戏中按 P

```bash
./build/fcemu /path/to/rom.nes
```

- 屏上 toast: `Photo: repainting...`
- 后台异步推理 ≈ 90 s（M2 + ControlNet）
- 完成时 toast: `Photo saved: <name>`，PNG 写到 `~/Desktop/fcemu-photo/`

## 环境变量（可选）

| 变量 | 默认 | 说明 |
|---|---|---|
| `FCEMU_PHOTO_SOCKET` | `/tmp/fcemu_photo.sock` | daemon 套接字路径 |
| `FCEMU_PHOTO_PROMPT` | 脚本默认 | 全局提示词覆盖（C++ 端） |
| `FCEMU_PHOTO_SCRIPT` | – | 无 daemon 时的备援脚本路径 |
| `FCEMU_PHOTO_PYTHON` | – | 无 daemon 时的备援 python 解释器 |
| `SDXL_TURBO_DIR`     | `~/sdxl-turbo-local`        | SDXL Turbo 权重目录 |
| `SDXL_VAE_FP16_DIR`  | `~/sdxl-extras/vae-fp16-fix` | fp16-fix VAE 目录 |
| `CN_TILE_REPO`       | `~/sdxl-extras/cn-tile`     | ControlNet Tile 目录 |

## 性能（M2 24 GiB MPS）

| 模式 | 单帧延时 |
|---|---|
| no-CN, side=1024, steps=4 | ~40-60 s |
| ControlNet Tile, side=1024, steps=4 | ~90-130 s |
| pipeline 加载 | ~18 s |
| 1×1 warmup | ~45 s（首次 MPS 算子 JIT） |

效果对比图（生成时保留在桌面，不入库）：
- `~/Desktop/fcemu-photo-compare/C_strength_sweep.png` — 无 CN，strength 0.4-0.95
- `~/Desktop/fcemu-photo-compare/C_cn_vs_nocn.png` — ControlNet Tile 对结构的保护
