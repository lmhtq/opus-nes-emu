# FEAT-006: UI 与窗口管理#

## 元数据 (Metadata)

- **ID**: FEAT-006
- **关联模块 (Related Module)**: MOD-UI
- **关联需求 (Related Requirements)**: REQ-006
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30

## 功能描述 (Feature Description)

实现模拟器 UI，包括窗口、菜单、设置、状态保存等。

## 接口定义 (Interface Definition)

```cpp
class UI {
public:
    bool init();
    void run();  // main loop
    void render_frame(const uint8_t* framebuffer);
    void load_rom(const std::string& path);
    void set_state(EmuState state);
};
```

## 流程图 (Flow Chart)

```
[SDL_Init]
    → [Create Window + OpenGL Context]
        → [Init ImGui]
            → [Main Loop]:
                → [Process SDL Events]
                    → [If ROM loaded: emulator.step()]
                        → [Render PPU frame]
                            → [Video Enhancer: apply effects]
                                → [Render ImGui overlay]
                                    → [Swap Window]
```

## 边界条件 (Edge Cases)

1. **OpenGL 不支持**：回退到软件渲染
2. **窗口最小化**：暂停模拟
3. **无效 ROM**：弹窗提示
4. **设置损坏**：使用默认值

## 测试场景 (Test Scenarios)

1. 窗口创建正确（512x480 或更大）
2. NES 画面正确显示（可缩放）
3. 菜单功能完整
4. ROM 加载后自动运行
5. 暂停/运行/重置正确
6. 逐帧前进正确
7. 速度控制（0.5x/1x/2x/4x）
8. 全屏切换正确
9. 即时存档 10 个槽位
10. 设置持久化

## 关联硬件文档 (Related Hardware Docs)

- 无（UI 是模拟器自身功能）

## 变更记录 (Change History)

- 2026-04-30: Initial version
