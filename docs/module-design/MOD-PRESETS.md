# MOD-PRESETS: 预制包管理#

## 元数据 (Metadata)

- **ID**: MOD-PRESETS
- **关联概要设计 (Related Overview)**: OVERVIEW-001
- **关联需求 (Related Requirements)**: REQ-105, REQ-110
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30
- **最后更新 (Updated)**: 2026-04-30

## 功能职责 (Responsibilities)

实现预制包管理系统。

核心职责：
1. ROM 哈希识别（SHA-256）
2. 预制包自动匹配（根据 ROM 哈希）
3. 预制包格式定义（manifest.json）
4. 预制包管理（浏览/启用/禁用/删除）
5. 预制包导入（本地文件或在线仓库）
6. 版本管理（预制包版本与模拟器兼容性）
7. 视觉包和音频包统一管理

## 接口设计 (Interface Design)

```cpp
// include/fcemu/presets.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <json/json.h>

namespace fcemu {

// 预制包类型
enum class PresetType { Video, Audio, All };

// 替换项
struct Replacement {
    std::string type;           // "tile", "sprite", "background", "music", "sfx"
    std::string original_id;      // 原始资源标识
    std::string replacement_file; // 替换文件路径
    std::map<std::string, std::string> params;  // 额外参数
};

// 预制包 manifest
struct PresetManifest {
    std::string name;
    std::string version;
    std::string game;
    std::string rom_sha256;
    std::string author;
    std::string description;
    PresetType type;
    std::vector<Replacement> replacements;

    // 序列化
    static PresetManifest from_json(const std::string& json_str);
    std::string to_json() const;
};

// 预制包信息
struct PresetInfo {
    std::string id;          // 唯一 ID（通常是目录名）
    PresetManifest manifest;
    std::string path;        // 包目录路径
    bool enabled = false;
    bool compatible = true;   // 与当前模拟器版本兼容
};

// 预制包管理器
class PresetManager {
public:
    PresetManager();

    bool init(const std::string& presets_dir);
    void shutdown();

    // 扫描可用预制包
    void scan_presets();
    const std::vector<PresetInfo>& presets() const { return presets_; }

    // 匹配预制包（根据 ROM SHA-256）
    PresetInfo* find_matching_preset(const std::string& rom_sha256, PresetType type);
    std::vector<PresetInfo*> find_all_matching(const std::string& rom_sha256);

    // 启用/禁用
    bool enable_preset(const std::string& preset_id);
    bool disable_preset(const std::string& preset_id);
    bool is_enabled(const std::string& preset_id) const;

    // 导入预制包
    bool import_preset(const std::string& path);  // 本地 zip 或目录
    bool import_preset_url(const std::string& url);  // 在线下载

    // 删除预制包
    bool remove_preset(const std::string& preset_id);

    // 获取替换项（给 VideoEnhancer/AudioEnhancer 使用）
    std::string get_replacement(const std::string& original_id,
                               const std::string& type) const;

    // 验证预制包
    bool validate_preset(const PresetManifest& manifest) const;

    // 创建空预制包模板
    bool create_template(const std::string& game_name,
                         const std::string& rom_sha256,
                         PresetType type);

private:
    std::string presets_dir_;
    std::vector<PresetInfo> presets_;

    // 已启用的预制包
    std::map<std::string, PresetInfo*> enabled_presets_;

    // 辅助
    bool load_manifest(const std::string& preset_path, PresetManifest& out);
    std::string compute_sha256(const std::vector<uint8_t>& data) const;
};

} // namespace fcemu
```

## 依赖关系 (Dependencies)

| 依赖模块 | 说明 |
|----------|------|
| MOD-RESOURCE | 获取资源清单用于预制包创建 |
| MOD-VIDEO | 应用视觉替换 |
| MOD-AUDIO | 应用音频替换 |
| MOD-UI | 预制包管理界面 |

## 关联硬件文档 (Related Hardware Docs)

- 无（预制包是模拟器扩展功能）

## 变更记录 (Change History)

- 2026-04-30: Initial version
