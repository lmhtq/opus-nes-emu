# iNES ROM 格式参考

FC/NES ROM 文件通常使用 iNES 格式（扩展名 `.nes`）。

## iNES 文件头（16 字节）

| 偏移 | 大小 | 说明 |
|------|------|------|
| 0-3 | 4B | 魔术字符串 "NES\x1a" |
| 4 | 1B | PRG ROM 大小（单位：16KB banks） |
| 5 | 1B | CHR ROM 大小（单位：8KB banks），0 表示 CHR RAM |
| 6 | 1B | 标志位 6（详见下文） |
| 7 | 1B | 标志位 7（详见下文） |
| 8 | 1B | PRG RAM 大小（单位：8KB，0 表示 8KB） |
| 9 | 1B | 标志位 9 |
| 10 | 1B | 标志位 10 |
| 11-15 | 5B | 保留（全 0） |

## 标志位 6（byte 6）

| 位 | 名称 | 说明 |
|----|------|------|
| 0 | Mirroring | 0=水平（Vertical mirror），1=垂直（Horizontal mirror） |
| 1 | Battery | 1=卡带有电池备份 RAM |
| 2 | Trainer | 1=文件包含 512 字节 trainer（在 PRG ROM 之前） |
| 3 | Four Screen | 1=四屏幕镜像（忽略位 0） |
| 4-7 | Mapper Low | Mapper 编号的低 4 位 |

## 标志位 7（byte 7）

| 位 | 名称 | 说明 |
|----|------|------|
| 0-3 | Mapper High | Mapper 编号的高 4 位 |
| 4 | VS Unisystem | 1=VS 系统卡带 |
| 5 | PlayChoice-10 | 1=PlayChoice-10 卡带 |
| 6-7 | 未使用 | 始终为 0 |

## Mapper 编号计算

```
Mapper = (byte[6] >> 4) | (byte[7] & 0xF0)
```

示例：
- byte[6] = 0x01, byte[7] = 0x00 → Mapper = 1（MMC1）
- byte[6] = 0x02, byte[7] = 0x00 → Mapper = 2（UxROM）
- byte[6] = 0x04, byte[7] = 0x00 → Mapper = 4（MMC3）

## 文件布局

```
+-------------------+
| iNES Header (16B) |
+-------------------+
| Trainer (512B)    |  (optional, if byte[6] bit 2=1)
+-------------------+
| PRG ROM (16KB x N)|
+-------------------+
| CHR ROM (8KB x M) |
+-------------------+
```

## PRG ROM

- 16KB 为单位（1 bank = 16KB）
- 映射到 CPU 地址 $8000-$FFFF
- Mapper 0：16KB 时 $8000-$BFFF 和 $C000-$FFFF 镜像同一 bank；32KB 时直接映射
- 其他 Mapper：bank 切换

## CHR ROM/RAM

- 8KB 为单位（1 bank = 8KB）
- 映射到 PPU 地址 $0000-$1FFF
- 0 表示使用 CHR RAM（需模拟器提供 8KB RAM）
- CHR RAM 通常用于游戏自行绘制图形

## 示例 ROM 分析

### 《超级马里奥兄弟》(Mapper 0)

```
Header: "NES\x1a" 02 01 01 00 ...
- PRG ROM: 2 x 16KB = 32KB
- CHR ROM: 1 x 8KB = 8KB
- Mapper: 0 (NROM)
- Mirroring: 垂直（水平镜像）
```

### 《洛克人 2》(Mapper 4)

```
Header: "NES\x1a" 08 10 41 00 ...
- PRG ROM: 8 x 16KB = 128KB
- CHR ROM: 16 x 8KB = 128KB
- Mapper: 4 (MMC3)
- Battery: 有电池备份
```

## 参考来源

- [NESDEV Wiki - iNES](https://www.nesdev.org/wiki/INES)
- [NESDEV Wiki - NES 2.0](https://www.nesdev.org/wiki/NES_2.0)
