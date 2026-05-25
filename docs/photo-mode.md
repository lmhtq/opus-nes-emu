# Photo Mode (REQ-117)

游戏里按 **P** 键，把当前 PPU 帧（256×240）通过云端生图大模型 API 重绘成高分辨率画面，**同时保留原始 256×240 PPU 帧**作并排对比，落到项目根的 `fcemu-photo/`（可用 `FCEMU_PHOTO_DIR` 改路径）。

每次按 P 产出一对同时间戳前缀的文件：

```
fcemu-photo/SuperMarioBros_20260526-143012_orig.png   # 256×240 原图
fcemu-photo/SuperMarioBros_20260526-143012.png        # 云端 API 重绘
```

实现：fcemu 通过 Unix socket `/tmp/fcemu_photo.sock` 推送请求给一个常驻的 Python daemon。daemon 是一个**多 provider 的 HTTPS 客户端**——本身不跑模型，只负责把帧上传到云端 API，把结果写回磁盘。原图由 C++ 侧直接写盘（不经 daemon），所有 provider 行为一致。

## 当前支持的 provider

| backend  | 模型 / 服务                         | 必需环境变量      | 备注 |
|----------|--------------------------------------|-------------------|------|
| `ark`    | 豆包 Seedream / SeedEdit（火山方舟） | `ARK_API_KEY`     | 默认 `doubao-seedream-4-0-250828`。模型 ID 用 `ARK_IMAGE_MODEL` 或 `--model` 覆盖。 |
| `openai` | OpenAI Images Edit（gpt-image-1）    | `OPENAI_API_KEY`  | 多模态图像编辑，输入会被 letterbox 到正方形。 |

### Ark 可用模型 ID（按需自取）

火山方舟控制台**「在线推理 → 模型广场」**里 inference endpoint 名就是这里要填的 `model`。当前可直接复制粘贴：

| 系列 | endpoint ID | 说明 |
|---|---|---|
| **Seedream 5.0** | `doubao-seedream-5-0-260128` | 当前最强，画质 / 细节 / 文字渲染最好，单帧成本最高 |
| **Seedream 4.0** | `doubao-seedream-4-0-250828` | 主力多模态 t2i + i2i，性价比首选，默认值 |
| Seedream 3.0 t2i | `doubao-seedream-3-0-t2i-250415` | 纯文生图老版 |
| SeedEdit 3.0 i2i | `doubao-seededit-3-0-i2i-250628` | 老 i2i，逐步下线，仅在指定地区可用 |

切换方式（三档优先级，前者覆盖后者）：

```bash
# 1) 单请求级（C++ 端将来可以扩展协议，curl 也能这样测）
echo '{"in":"frame.png","out":"out.png","prompt":"...","model":"doubao-seedream-5-0-260128"}' \
  | nc -U /tmp/fcemu_photo.sock

# 2) CLI 旗标（daemon 或单帧都行）
./tools/photo/photo_repaint.py --daemon --backend=ark --model=doubao-seedream-5-0-260128

# 3) 环境变量（最省事，启动前 export 一次）
export ARK_IMAGE_MODEL=doubao-seedream-5-0-260128
./tools/photo/photo_repaint.py --daemon --backend=ark
```

> 404 `InvalidEndpointOrModel.NotFound` = 你的账号没开通这个 endpoint 或拼错了。去控制台复制 endpoint 名再贴。

加新 provider 是 `photo_repaint.py` 里增加一个 `Provider` 子类（实现 `repaint()`）并注册到 `PROVIDERS` 字典——通常 ~40 行。

## 一次性安装

```bash
cd tools/photo
python3 -m venv .venv
.venv/bin/pip install -U pip
.venv/bin/pip install -r requirements.txt
```

依赖只有 `requests + Pillow`，秒级完成，不再下任何模型权重。

## 启动

### 1. 设置 API key

```bash
export ARK_API_KEY=xxx                          # 豆包
# 或
export OPENAI_API_KEY=sk-xxx                    # OpenAI
```

### 2. 启 daemon（一次即可，常驻避免每次启动）

```bash
nohup tools/photo/.venv/bin/python tools/photo/photo_repaint.py \
  --daemon --backend=ark \
  > ~/Desktop/fcemu-photo/daemon.log 2>&1 &
```

日志出现 `[photo-daemon] listening on /tmp/fcemu_photo.sock` 即就绪。
**没有 18 s 模型加载、没有 45 s warmup**——daemon 启动几乎是即时的。

### 3. 启 fcemu，游戏中按 P

```bash
./build/fcemu /path/to/rom.nes
```

- 屏上 toast：`Photo: repainting...`
- 后台异步上传 + 推理：豆包 SeedEdit 一般 10–30 s
- 完成时 toast：`Photo saved: <name>`，PNG 写到 `~/Desktop/fcemu-photo/`

## 命令行单帧调试

不走 daemon，也可以一次性跑：

```bash
tools/photo/.venv/bin/python tools/photo/photo_repaint.py \
  --backend=ark \
  --in /tmp/some_frame.png \
  --out /tmp/repainted.png \
  --prompt "photorealistic 1980s Mario in the Mushroom Kingdom, cinematic, 35mm film"
```

## Daemon 协议

C++ 端 (`launch_photo_repaint` in `src/main.cpp`) 通过 UNIX socket 发一行 JSON，daemon 回一行 JSON：

```jsonc
// request
{"in": "/tmp/fcemu_photo_xxx.png",
 "out": "~/Desktop/fcemu-photo/rom_xxx.png",
 "prompt": "...",
 "backend": "ark",          // 可选，覆盖 daemon 默认
 "model": "doubao-seedream-4-0-250828",  // 可选，按请求换模型
 "seed": 42,                // 可选
 "size": "2K",              // 可选（"1024x1024" / "2K" / "adaptive"）
 "guidance_scale": 5.5}     // 可选（SeedEdit 3.x only，新模型不要传）

// reply
{"ok": true, "out": "...", "ms": 12345, "backend": "ark"}
// 或
{"ok": false, "error": "ArkProvider: HTTP 401: ..."}
```

C++ 端只解析 `"ok": true`，回包多余字段被忽略——加新字段是向后兼容的。

## 环境变量（可选）

| 变量 | 默认 | 说明 |
|---|---|---|
| `FCEMU_PHOTO_DIR`        | `./fcemu-photo` | 照片输出目录（C++ 端） |
| `FCEMU_PHOTO_SOCKET`     | `/tmp/fcemu_photo.sock` | daemon 套接字路径（C++ + Python 共用） |
| `FCEMU_PHOTO_PROMPT`     | provider 默认 | 全局 prompt 覆盖（C++ 端） |
| `FCEMU_PHOTO_BACKEND`    | `ark`  | `--daemon` 不带 `--backend` 时的默认 |
| `ARK_API_KEY`            | – | 豆包必填 |
| `ARK_IMAGE_MODEL`        | `doubao-seedream-4-0-250828` | 覆盖 Ark 模型 ID（Seedream 4.5/5、SeedEdit 3 等都在这里换） |
| `OPENAI_API_KEY`         | – | OpenAI 必填 |
| `OPENAI_IMAGE_MODEL`     | `gpt-image-1` | 覆盖 OpenAI 模型 ID |

## 性能 / 计费

| 提供商 | 单帧延时（含上传 + 推理 + 下载） | 备注 |
|---|---|---|
| Ark SeedEdit (1024×1024) | ~10–30 s | 国内访问稳定 |
| OpenAI gpt-image-1 edit  | ~15–40 s | 需要海外网络 |

按次计费由各提供商账单决定，本仓库不缓存任何 key。

## 添加新 provider

例如想加 SiliconFlow / DashScope / 智谱 CogView：

1. 在 `photo_repaint.py` 增加：
   ```python
   class MyProvider(Provider):
       name = "myx"
       def __init__(self):
           self.api_key = os.environ.get("MYX_API_KEY", "")
           if not self.api_key: raise RuntimeError("MYX_API_KEY not set")
       def repaint(self, in_path, out_path, *, prompt, **opts):
           ...
           return int((time.time() - t0) * 1000)
   ```
2. 在 `PROVIDERS = {...}` 注册 `"myx": MyProvider`。
3. 启动 `--backend=myx` 即可。

无需改 C++、无需改协议。
