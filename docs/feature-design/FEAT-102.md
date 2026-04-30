# FEAT-102: 高清纹理替换

## 元数据 (Metadata)

- **ID**: FEAT-102
- **关联模块 (Related Module)**: MOD-VIDEO
- **关联需求 (Related Requirements)**: REQ-102
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30

## 功能描述 (Feature Description)

Tile 级的高清纹理替换系统，支持用户提供的 PNG 图片替换原始 8x8 Tile。

## 接口定义 (Interface Definition)

```cpp
struct TextureReplacement {
    uint16_t chr_bank;       // CHR bank (0 or 1 for 8x16 sprites)
    uint8_t tile_id;        // Tile index (0-255)
    uint8_t palette;         // Palette (0-3, optional)
    uint32_t gl_texture;     // OpenGL texture handle
    int width, height;      // 替换纹理尺寸（如 32x32）
};

class TextureReplacer {
public:
    bool load_replacement(const std::string& manifest_path);
    bool get_replacement(uint16_t bank, uint8_t tile,
                          uint8_t palette,
                          TextureReplacement& out) const;
    void clear();
    bool hot_reload();  // 重新加载所有替换文件
};
```

## 流程图 (Flow Chart)

```
[Load ROM]
    → [Analyze Resources: extract CHR tiles]
        → [Load Preset Manifest: read manifest.json]
            → [For each replacement in manifest]:
                → [Load PNG file]
                    → [Create OpenGL texture]
                        → [Store in replacement map]
                            → [During rendering: lookup replacement]
                                → [Use HD texture if found, else original]
```

## 边界条件 (Edge Cases)

1. **调色板匹配**：同一 Tile 不同调色板对应不同高清纹理
2. **无替换**：找不到替换时使用原始渲染
3. **PNG 尺寸**：任意尺寸 PNG，自动缩放到合适大小
4. **无效 PNG**：跳过该替换项，记录警告
5. **热加载**：文件修改后自动重新加载（inotify/轮询）
6. **内存限制**：限制替换纹理总大小（如最大 256MB）

## 测试场景 (Test Scenarios)

1. **Tile 提取**：从 CHR ROM 正确提取所有 Tile（8x8, 2bpp）
2. **PNG 替换**：用户 PNG 文件正确替换对应 Tile
3. **调色板感知**：同一 Tile 不同调色板使用不同纹理
4. **热加载**：修改 PNG 文件后 ≤ 1 秒生效
5. **原始/替换切换**：实时切换，画面正确更新
6. **精灵替换**：精灵 Tile 正确替换
7. **背景替换**：背景 Tile 正确替换
8. **无效文件处理**：损坏的 PNG 不会崩溃
9. **manifest.json 格式**：正确解析，错误报告
10. **无替换降级**：找不到替换时降级到原始渲染

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/ppu/rendering.md`
- `docs/hardware/ppu/sprites.md`

## 变更记录 (Change History)

- 2026-04-30: Initial version
