# FEAT-003: APU 音频通道模拟#

## 元数据 (Metadata)

- **ID**: FEAT-003
- **关联模块 (Related Module)**: MOD-APU
- **关联需求 (Related Requirements)**: REQ-003
- **状态 (Status)**: Draft
- **创建日期 (Created)**: 2026-04-30

## 功能描述 (Feature Description)

实现 APU 的 5 个音频通道（2 脉冲波、1 三角波、1 噪声、1 DMC）。

## 接口定义 (Interface Definition)

```cpp
class Apu {
public:
    void step(int cpu_cycles);
    int16_t mix_output() const;
    void generate_samples(std::vector<int16_t>& out, int num_samples);
};
```

## 流程图 (Flow Chart)

```
[For each APU cycle]:
    → [Clock Pulse 1: envelope/sweep/length]
        → [Clock Pulse 2: envelope/sweep/length]
            → [Clock Triangle: linear/length]
                → [Clock Noise: length/shift]
                    → [Clock DMC: sample gen]
                        → [Mix: pulse1 + pulse2 + tri + noise + dmc]
                            → [Output samples]
```

## 边界条件 (Edge Cases)

1. **长度计数器到 0**：通道静音
2. **DMC IRQ**：采样结束触发 IRQ
3. **帧计数器模式**：4 步/5 步不同
4. **脉冲波占空比**：0/12.5%/25%/50%/75%
5. **噪声模式**：短周期（15-bit）/长周期（1-bit）

## 测试场景 (Test Scenarios)

1. 脉冲波占空比正确（4 种）
2. 脉冲波包络衰减正确（15→0）
3. 脉冲波扫频正确（频率上下扫描）
4. 三角波输出正确（4-bit DAC）
5. 噪声通道正确（短/长周期）
6. DMC 播放采样正确
7. 帧计数器 4 步/5 步正确
8. 混音输出合理
9. APU 状态寄存器正确
10. DMC IRQ 正确触发

## 关联硬件文档 (Related Hardware Docs)

- `docs/hardware/apu/registers.md`
- `docs/hardware/apu/audio-channels.md`

## 变更记录 (Change History)

- 2026-04-30: Initial version
