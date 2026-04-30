# FEAT-101: CRT 画质增强

## 元数据 (Metadata)

- **ID**: FEAT-101
- **关联模块 (Related Module)**: MOD-VIDEO
- **关联需求 (Related Requirements)**: REQ-101
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30

## 功能描述 (Feature Description)

实现 CRT 显示器效果，包括扫描线、曲率、光晕、色散、HDR 色调映射、抗锯齿等。

## 接口定义 (Interface Definition)

```cpp
// OpenGL Shader 输入
struct CrtInputs {
    uint32_t source_texture;   // 256x240 RGBA 原始画面
    int width = 256;
    int height = 240;
};

// CRT 效果参数（见 MOD-VIDEO 的 CRTEffect）
// 输出：处理后的 256x240 RGBA 纹理

class VideoEnhancer {
public:
    uint32_t apply_crt(uint32_t source_tex);
    // 内部：编译并运行 CRT shader
};
```

## 流程图 (Flow Chart)

```
[Original 256x240 Frame]
    → [CRT Shader Pass 1: Scanlines]
        → [CRT Shader Pass 2: Curvature + Distortion]
            → [CRT Shader Pass 3: Bloom / Glow]
                → [CRT Shader Pass 4: Chromatic Aberration]
                    → [Tone Mapping: HDR -> LDR]
                        → [Anti-Aliasing: FXAA / SMAA]
                            → [Output Enhanced Frame]
```

## 边界条件 (Edge Cases)

1. **低性能模式**：效果强度自动降低以保证帧率
2. **无效纹理**：源纹理未就绪时返回黑色帧
3. **参数越界**：强度/半径等参数自动钳位到有效范围
4. **OpenGL 不支持**：回退到软件渲染（无 CRT 效果）
5. **HDR 无效输入**：色调映射使用默认曝光值

## 测试场景 (Test Scenarios)

1. **扫描线效果**：开启后可见水平暗线，强度可调
2. **曲率效果**：画面边缘弯曲，曲率可调
3. **光晕效果**：亮区向周围扩散发光
4. **色散效果**：RGB 通道轻微偏移（色差）
5. **HDR 色调映射**：非常亮/非常暗的场景正确显示
6. **抗锯齿**：边缘平滑，无锯齿
7. **预设模式**：轻度/中度/重度 CRT 一键切换
8. **实时开关**：开启/关闭效果无需重启
9. **性能测试**：开启所有效果帧率下降 ≤ 20%
10. **亮度/对比度/饱和度**：滑块调节正确，画面实时更新

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/ppu/rendering.md`

## 变更记录 (Change History)

- 2026-04-30: Initial version
