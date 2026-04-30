# FEAT-115: 游戏资源分析器#

## 元数据 (Metadata)

- **ID**: FEAT-115
- **关联模块 (Related Module)**: MOD-RESOURCE
- **关联需求 (Related Requirements)**: REQ-115
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30#

## 功能描述 (Feature Description)

实现游戏资源分析器，自动提取可替换的资源。

## 接口定义 (Interface Definition)

```cpp
class ResourceAnalyzer {
public:
    bool analyze_rom(const std::vector<uint8_t>& prg,
                     const std::vector<uint8_t>& chr,
                     const RomInfo& info);
    const ResourceManifest& manifest() const;
    void export_manifest(const std::string& path) const;
};
```

## 流程图 (Flow Chart)

```
[Load ROM]
    → [Parse CHR ROM: extract all tiles (8x8, 2bpp)]
        → [Analyze APU patterns: identify music tracks]
            → [Analyze OAM patterns: identify sprites]
                → [Read PPU palette: $3F00-$3F1F]
                    → [Generate ResourceManifest (JSON)]
                        → [Export to manifest.json]
```

## 边界条件 (Edge Cases)

1. **CHR RAM**：无法提取，记录为空
2. **未知游戏**：通用分析
3. **资源清单过大**：分页显示
4. **导出失败**：磁盘满提示

## 测试场景 (Test Scenarios)

1. ROM 正确分析（至少 20 个常见游戏）
2. 音乐正确提取（识别 Pulse/Triangle 旋律）
3. 音效正确提取（至少 10 种）
4. Tile 正确提取（8x8, 2bpp）
5. 精灵正确提取（8x8 或 8x16）
6. 调色板正确提取
7. 资源清单格式清晰
8. 资源预览功能
9. 分析准确率 > 90%
10. 分析速度 < 5 秒

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/cpu/instruction-set.md`
- `docs/hardware/ppu/` 所有章节
- `docs/hardware/apu/` 所有章节

## 变更记录 (Change History)

- 2026-04-30: Initial version
