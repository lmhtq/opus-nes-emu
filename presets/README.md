# 预制资源包 (Preset Packs)

本目录存放预制资源包，用于替换 FC 游戏的原始资源。

## 目录结构

```
presets/
├── audio/                   # 预制音频包
│   ├── super-mario-bros/  # 超级马里奥兄弟
│   │   ├── music/         # 替换音乐（remix 版本）
│   │   ├── sfx/           # 替换音效
│   │   └── manifest.json  # 包描述文件
│   └── contra/            # 魂斗罗
│       └── ...
├── video/                   # 预制视觉包
│   ├── super-mario-bros/  # 超级马里奥兄弟
│   │   ├── tiles/         # 高清 Tile 替换
│   │   ├── sprites/       # 高清精灵替换
│   │   ├── backgrounds/   # 高清背景替换
│   │   └── manifest.json  # 包描述文件
│   └── contra/
│       └── ...
└── README.md                # 本文件
```

## manifest.json 格式

```json
{
  "name": "Super Mario Bros. HD Pack",
  "version": "1.0.0",
  "game": "Super Mario Bros.",
  "rom_hash": "SHA-256 hash of the ROM",
  "author": "Author Name",
  "description": "HD textures and remix audio for SMB",
  "replacements": [
    {
      "type": "tile",
      "original_bank": 0,
      "original_tile": 0,
      "replace_file": "tiles/ground.png"
    },
    {
      "type": "music",
      "original_track": "overworld",
      "replace_file": "music/overworld_remix.mp3"
    }
  ]
}
```

## 使用方式

1. 将预制包放入对应目录（`audio/` 或 `video/`）
2. 启动 fcemu 并加载对应 ROM
3. 模拟器通过 ROM 哈希自动匹配预制包
4. 也可在设置中手动选择预制包

## 制作预制包

使用 `src/resource/` 模块的资源分析器可以：
1. 自动提取游戏资源（Tile、精灵、音乐、音效）
2. 生成资源清单（manifest.json 模板）
3. 用户替换资源后打包

详见 [资源分析器文档](../docs/specs/REQ-115.md)。
