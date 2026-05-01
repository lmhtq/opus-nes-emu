# FEAT-116: AI 超分 PoC（Mac / Real-ESRGAN ncnn 子进程）

## 元数据 (Metadata)

- **ID**: FEAT-116
- **关联模块 (Related Module)**: MOD-VIDEO-AIUPSCALE
- **关联需求 (Related Requirements)**: REQ-116
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-05-01

## 功能描述 (Feature Description)

提供 `IAiUpscaler` 抽象接口与两个后端实现，并提供 `fcemu_ai_upscale_demo` 工具
完成"ROM → 60 帧 PPU 输出 → AI 4× 超分 → side-by-side 对比 PNG + 分阶段耗时"
的完整 PoC 链路。

## 接口定义 (Interface Definition)

见 `src/include/fcemu/ai_upscaler.h`（详见 MOD-VIDEO-AIUPSCALE）。

工具 CLI：

```
fcemu_ai_upscale_demo <rom> [options]
  --model NAME            默认 realesr-animevideov3
  --model-dir DIR         默认 $FCEMU_AIUP_MODEL_DIR 或 ./models
  --bin PATH              默认 $FCEMU_AIUP_BIN 或 PATH 中 realesrgan-ncnn-vulkan
  --frames N              boot 后跑的总帧数（默认 120）
  --capture-from N        从第 N 帧开始抓（默认 60，跳过启动黑屏）
  --capture-count N       抓取帧数（默认 1）
  --every K               每 K 帧抓一次（默认 1）
  --out-dir DIR           输出目录（默认 ./aiup_out）
  --backend NAME          nearest | ncnn-subprocess（默认 ncnn-subprocess）
  --keep-temp             保留中间 tmpdir
```

## 流程图 (Flow Chart)

```
[CLI 解析]
   │
   ▼
[加载 ROM → boot CPU/PPU/APU/Mem (复用 headless_runner)]
   │
   ▼
[run frames; 在指定帧抓取 PPU.frame().pixels]
   │
   ▼
[Frame[] in-memory]
   │
   ├──► 写 PNG 到 tmp/in/frame_000001.png …    (timing: write_png_ms)
   │
   ▼
[fork+exec realesrgan-ncnn-vulkan -i tmp/in -o tmp/out -n MODEL -s SCALE -m MODEL_DIR]
                                                (timing: subprocess_ms)
   │
   ▼
[读 PNG tmp/out/frame_000001.png …]            (timing: read_png_ms)
   │
   ▼
[生成对比图：左=Nearest 4×，右=AI 输出]         (timing: montage_ms)
   │
   ▼
[打印分阶段耗时表 + 写 timing.json]
```

## 边界条件 (Edge Cases)

1. ROM 文件无法读取 → 退出码 2
2. 二进制/模型缺失 → 退出码 3，stderr 给出探测过程
3. 子进程超时（默认 60 s）→ 终止并返回 false
4. PNG 写盘失败（磁盘满） → 返回 false
5. 输出帧少于输入 → 视为失败，保留 tmpdir 供排查
6. 帧 RGBA 含非 255 alpha → 写 PNG 前强制覆盖为 255

## 测试场景 (Test Scenarios)

| ID | 场景 | 期望 |
|----|------|------|
| TS-1 | NearestNeighborUpscaler 4× | 输出 1024×960，与 stb 最近邻一致 |
| TS-2 | NcnnSubprocessUpscaler init 缺失 binary | 返回 false，错误信息含 "binary not found" |
| TS-3 | NcnnSubprocessUpscaler 单帧 SMB title | 输出 1024×960，文件头为 PNG |
| TS-4 | 批量 16 帧 | 顺序与输入一致，所有输出尺寸正确 |
| TS-5 | end-to-end demo on SMB | 生成 montage_*.png 与 timing.json |
| TS-6 | timing 拆分合理 | 4 个阶段耗时之和 ≤ 总耗时 + 5% |

## 性能预算（M2 base，60 帧 ×4）

| 阶段 | 预算 |
|------|------|
| write_png | ≤ 200 ms |
| subprocess (ncnn 推理) | ≤ 4500 ms |
| read_png | ≤ 600 ms |
| montage | ≤ 300 ms |
| 合计 | ≤ 6 s（验收标准 #10） |

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/ppu/rendering.md`

## 变更记录 (Change History)

- 2026-05-01: Initial version
