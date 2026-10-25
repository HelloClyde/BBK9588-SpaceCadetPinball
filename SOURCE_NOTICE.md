# 来源与许可

本仓库的自有移植代码、脚本和文档按根目录 MIT 许可证提供。
本项目为非官方移植，与 BBK、Microsoft 或原游戏权利人无隶属关系。

| 内容 | 来源与固定版本 | 许可 / 范围 |
| --- | --- | --- |
| `native/engine/` 原版开源引擎 | [k4zmu2a/SpaceCadetPinball](https://github.com/k4zmu2a/SpaceCadetPinball/tree/cb9b7b886244a27773f66b0b19fdc2998392565e)，`cb9b7b886244a27773f66b0b19fdc2998392565e` | MIT，保留 `native/engine/LICENSE`；已替换平台、部分头文件及翻译适配 |
| `sdk/` | [HelloClyde/bbk9588-bda-sdk](https://github.com/HelloClyde/bbk9588-bda-sdk)，`f2fcbe020e7e94014c33d7e5c593802e4b1e3034` | SDK 自带 Apache-2.0，子模块固定提交 |
| `native/sdk_compat.h` | SDK 开发工作区已验证的 FS+0x074 刷盘封装 | Apache-2.0，见 `licenses/Apache-2.0.txt` 与 `licenses/SDK-NOTICE.txt`；只保留此接口，未依赖其他未提交 SDK 改动 |
| 原版 DAT、位图、字体、MIDI、60 个 WAV | 固定资源清单见 `assets/original-resources.json`；先前验证来源为 [lrusso/3DPinballSpaceCadet](https://github.com/lrusso/3DPinballSpaceCadet/tree/684f0b57d0cc93d5a29329f0b59d9996c54f1553) | 原游戏第三方素材，未确认独立再分发许可；不包含于源码或默认安装包，不套用 MIT / Apache-2.0 |
| `windows_xp_pinball.ico`、`original_icon.png` 与 BDA 菜单图标 | 经哈希核验的 Windows XP `PINBALL.EXE` 静态提取 | 原游戏第三方图标；未确认独立再分发许可，**不包含在 MIT 授权中** |
| `docs/screenshots/` | 本移植在完整 C200 固件模拟器中的实际截图 | 用于运行展示；其中原游戏美术仍属原权利人，不由本仓库重新授权 |

原版图标遵照原版界面要求保留。该素材的独立再分发许可未确认；
代码开源许可不能替代原图标的授权，不对该素材臆造 MIT 或 Apache-2.0 授权。
原程序、固件、NAND、工具链、存档和游戏数据均未纳入本应用 Git。

原 EXE SHA-1：`2a5b525e0f631bb6107639e2a69df15986fb0d05`；仅解析 PE 资源，从未执行。
原 ICO SHA-256：`1ebdd10aad7ff3f465eeafa285a06e448788b5b89ab6a212113e58435db7c16e`。
提取记录在 `docs/verification/xp-exe-provenance.json` 和 `xp-icons.json`。
ICO 含 16/32/48 像素、4/8/32 bpp 共九帧；构建使用原始 48 像素 32 bpp 图像。

`PINBALL.DAT` SHA-256：`42defd5d2a339a3953fe643716aff4e35b2cea7a3e54505c82d1e78d5325bc04`。
`fetch_original_data.py` 仅在用户显式运行时下载固定来源，并校验整个 bundle 后静态提取；
不会执行下载脚本。下载地址和哈希证明来源与一致性，不代表素材获得开源授权。
