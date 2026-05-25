# roms/

把你自己的 `.nes` ROM 文件放在这个目录里。

```bash
./build/fcemu roms/your-game.nes
```

## 注意

- **本目录被 gitignore，任何 `.nes` 文件都不会被提交**——避免推送商业版权 ROM。
- 公共领域测试 ROM（如 nestest）也请自行下载，不入仓。
- 运行后产生的 `<rom>.sav`（电池 RAM）和 `<rom>.state`（存档）同样被忽略。

## 推荐获取

- **nestest.nes** — 6502 指令集回归测试，作者 kevtris，已公开发布。搜索 "nestest.nes" 即可。
- **homebrew ROMs** — itch.io / nesdev forums 有大量原创免费 ROM。

商业 ROM 请自行确认你拥有合法副本。
