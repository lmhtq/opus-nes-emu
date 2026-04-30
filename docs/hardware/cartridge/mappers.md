# 卡带映射器（Mapper）参考

FC/NES 游戏卡带使用不同的映射器（Mapper）来扩展地址空间。iNES 格式用 Mapper 编号来标识。

## 常见 Mapper

### Mapper 0 - NROM

| 属性 | 值 |
|------|-----|
| PRG ROM | 16KB 或 32KB |
| CHR ROM | 8KB（或 CHR RAM 8KB） |
| 镜像 | 水平或垂直 |
| 游戏示例 | 《超级马里奥兄弟》、《大金刚》 |

内存映射：
- $6000-$7FFF：PRG RAM（可选）
- $8000-$BFFF：PRG ROM（16KB，32KB ROM 时镜像）
- $C000-$FFFF：PRG ROM（16KB，32KB ROM 时 = $8000-$BFFF）

CHR：
- $0000-$1FFF：CHR ROM/RAM（8KB，直接映射）

### Mapper 1 - MMC1（SxROM）

| 属性 | 值 |
|------|-----|
| PRG ROM | 最多 256KB |
| CHR ROM/RAM | 最多 128KB ROM 或 8KB RAM |
| 镜像 | 可编程 |
| 游戏示例 | 《塞尔达传说》、《最终幻想》 |

特性：
- 通过串口（Serial Port）写入控制寄存器（5-bit 移位寄存器）
- PRG ROM 可切换 $8000-$BFFF 和/或 $C000-$FFFF
- CHR 可切换为 4KB banks
- 支持 PRG RAM 保护

控制寄存器地址：
| 地址范围 | 说明 |
|----------|------|
| $8000-$9FFF | 控制寄存器（镜像/CHR 模式） |
| $A000-$BFFF | CHR bank 0 |
| $C000-$DFFF | CHR bank 1 |
| $E000-$FFFF | PRG bank |

### Mapper 2 - UxROM

| 属性 | 值 |
|------|-----|
| PRG ROM | 16KB banks，最多 256KB |
| CHR | 8KB（通常为 CHR ROM） |
| 镜像 | 固定（水平或垂直） |
| 游戏示例 | 《超级马里奥兄弟》、《恶魔城》 |

特性：
- 通过写入 $8000-$FFFF 切换 $8000-$BFFF 的 bank
- $C000-$FFFF 固定为最后一个 bank
- 最简单且最常用的 Mapper 之一

### Mapper 3 - CNROM

| 属性 | 值 |
|------|-----|
| PRG ROM | 16KB 或 32KB |
| CHR ROM | 8KB banks，最多 32KB |
| 镜像 | 固定 |
| 游戏示例 | 《恶魔城》、《脱狱》 |

特性：
- 通过写入 $8000-$FFFF 切换 CHR bank（8KB）
- PRG ROM 固定

### Mapper 4 - MMC3（TxROM）

| 属性 | 值 |
|------|-----|
| PRG ROM | 最多 512KB |
| CHR ROM/RAM | 最多 256KB ROM 或 8KB RAM |
| 镜像 | 可编程 |
| 游戏示例 | 《洛克人 2》、《魂斗罗》 |

特性：
- 8KB PRG ROM bank 切换（可交换位置）
- 1KB 或 2KB CHR bank 切换（共 8 个 CHR banks）
- IRQ 计数器（扫描线计数，用于状态栏特效）
- 支持 PRG RAM 保护

寄存器（$8000-$FFFF）：
| 地址 (A0=0) | 说明 |
|--------------|------|
| $8000 | 控制寄存器（选择 bank 模式） |
| $8001 | 数据寄存器（写入 bank 编号） |
| $A000 | 镜像控制 |
| $A001 | PRG RAM 保护 |
| $C000 | IRQ 锁存器 |
| $C001 | IRQ 重装 |
| $E000 | IRQ 禁用 |
| $E001 | IRQ 使能 |

## Mapper 编号（iNES 格式）

iNES 头部 byte 6-7 的低位（bits 0-3 of byte 6，bits 0-3 of byte 7）组合成 Mapper 编号：
```
Mapper number = (byte[6] >> 4) | (byte[7] & 0xF0)
```

## 参考来源

- [NESDEV Wiki - Mapper](https://www.nesdev.org/wiki/Mapper)
- [NESDEV Wiki - List of mappers](https://www.nesdev.org/wiki/List_of_mappers)
