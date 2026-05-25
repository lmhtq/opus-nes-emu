# fcemu-photo/

Photo Mode (REQ-117) 的默认输出目录。在仓库根目录运行 `./build/fcemu` 时，按 **P** 触发的重绘照片会落到这里。

## 文件命名

每次按 P 在这个目录产生**两个文件**，共享时间戳前缀，Finder/ls 里相邻排序：

```
SuperMarioBros_20260526-143012_orig.png    # 原始 256×240 PPU 帧
SuperMarioBros_20260526-143012.png         # 云端 API 重绘后的高分辨率结果
```

`_orig.png` 用于和重绘结果做并排对比，直接看模型理解/改造了多少。

## 自定义输出目录

设置环境变量 `FCEMU_PHOTO_DIR`（绝对或相对路径都行）：

```bash
export FCEMU_PHOTO_DIR=~/Pictures/fc-photos
./build/fcemu roms/your-game.nes
```

## 入仓策略

这个目录被 gitignore 排除：所有 `.png` / `.log` 都不会被提交，只保留这份 README。生成的照片是你的本地数据，自行决定如何归档。
