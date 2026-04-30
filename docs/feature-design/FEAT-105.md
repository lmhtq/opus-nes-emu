# FEAT-105: 预制视觉包#

## 元数据 (Metadata)

- **ID**: FEAT-105
- **关联模块 (Related Module)**: MOD-PRESETS, MOD-VIDEO
- **关联需求 (Related Requirements)**: REQ-105
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30

## 功能描述 (Feature Description)

实现预制视觉包系统，ROM 哈希自动匹配并应用。

## 接口定义 (Interface Definition)

```cpp
class PresetManager {
public:
    bool load_preset(const std::string& manifest_path);
    PresetInfo* find_matching(const std::string& rom_sha256);
    bool enable_preset(const std::string& preset_id);
};
```

## 流程图 (Flow Chart)

```
[Load ROM]
    → [Compute SHA-256 of ROM]
        → [Scan presets/: find manifest.json with matching hash]
            → [If match: load manifest]
                → [Parse replacements: tile/sprite/background]
                    → [Load HD textures to GPU]
                        → [Enable replacements in VideoEnhancer]
```

## 边界条件 (Edge Cases)

1. **无匹配预制包**：使用原始渲染
2. **manifest 格式错误**：跳过，记录警告
3. **纹理文件缺失**：降级到原始渲染
4. **版本不兼容**：提示用户更新

## 测试场景 (Test Scenarios)

1. ROM 哈希计算正确（SHA-256）
2. 预制包自动匹配（哈希匹配）
3. manifest.json 格式正确
4. 至少 2 个示例预制包可用
5. 预制包管理界面功能完整
6. 手动选择预制包（自动匹配失败时）
7. 启用/禁用实时生效
8. 预制包与游戏版本匹配
9. 纹理替换正确应用
10. 性能影响小（帧率下降 ≤ 5%）

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/ppu/` 相关章节

## 变更记录 (Change History)

- 2026-04-30: Initial version
