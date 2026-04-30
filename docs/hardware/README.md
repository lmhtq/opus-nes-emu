# FC/NES 硬件参考文档

本目录包含 FC（Family Computer，即 NES）硬件的详细技术参考文档，作为 fcemu 模拟器实现的基础依据。

## 目录结构

| 路径 | 内容 |
|------|------|
| `cpu/` | 6502 CPU 参考 |
| `ppu/` | PPU（图片处理单元）参考 |
| `apu/` | APU（音频处理单元）参考 |
| `memory/` | 内存映射参考 |
| `cartridge/` | 卡带与映射器参考 |
| `input/` | 输入设备参考 |

## 使用方式

每个设计文档和实现代码应通过"关联硬件文档"字段引用对应的参考文档。例如：
- 模块设计 `MOD-CPU.md` 引用 `docs/hardware/cpu/instruction-set.md`
- 功能设计 `FEAT-xxx.md` 引用相关的硬件章节

## 数据来源

基于以下公开资料整理：
- MOS 6502 官方数据手册
- Nintendo NES 官方技术文档
- NESDEV Wiki (https://www.nesdev.org/wiki/)
