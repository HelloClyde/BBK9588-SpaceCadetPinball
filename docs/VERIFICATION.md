# 验证记录

日期：2026-10-25。SDK 固定为 `f2fcbe020e7e94014c33d7e5c593802e4b1e3034`，
已确认此提交可从公开 origin 按完整哈希获取。

## 原生构建与一致性

Windows / MIPS GCC 15.2.0，59 个编译单元，4 个 C++ 全局构造条目。
固定 SDK 加应用内 `sdk_compat.h` 后，重建 BDA 与原交付版逐字节一致：

```text
SpaceCadet9588.bda
size:   249448
SHA256: 61ce686b07777274d144d109119778414cb33814d3ae1b87cd5d569ed0f0683c
执行数据（从 0x95f8 开始）SHA256:
d7f8f22daa23d979a3a1d574a7c89c7bb515ed3a65ace9507e8c0df0f683e349
```

BDA 标题为“太空弹球”、分类为游戏，四档 VX 图标、入口、大小和 checksum 静态校验通过。
`build-native/build.json` 与交付 `dist/BUILDINFO.json` 记录实际编译器、SDK、上游和哈希。

已在仅含提交文件和固定 SDK 的干净检出中验证：

```powershell
.\sdk\scripts\setup_toolchain.ps1
.\build.ps1
python scripts/verify_checksums.py dist
```

工具链安装测试复用相同的本地缓存归档，经过脚本的 SHA-256 验证：
`8ba866e25c9826ee04ab4310365d264e3e73769e3738bb58ae38fd6740b7ee8d`。
构建没有读取原工作区或原版游戏数据。源码空白规范化后，BDA 仍与交付版完全一致。
本次重新运行的原版引擎宿主测试和 FILE/PAK 缓存行为测试均通过。
默认 ZIP 逐项检查不含 DAT、WAV、PAK、EXE、固件或个人存档。
README 截图和相对链接、Python 语法、PowerShell 构建语法、workflow YAML 结构均已检查。

## 复用的完整固件模拟器证据

环境：bbk9588-emulator v0.1.5、完整 C200 固件、独立 baseline v1.2.0 NAND 副本。
以公开 HTTP 输入、截图和文件导出接口测试，没有通过内存写补丁模拟成功。
原始完整测试 BDA SHA256 为
`a62c393029a9ea07dc06cb434095b37a5278192870ff071ad6c247bcfa94660e`。
之后只更新菜单图标，头部与游戏执行数据逐字节不变，
两次图标变更的证明保留在 `verification/icon-update-port-hires.json` 和 `icon-update.json`。
本次独立项目的完整 BDA 又与最终图标版逐字节一致，因此复用既有游戏运行证据。

- 原版 365×470 球台成功加载；进入球台 12.234 秒、球就位 18.421 秒，含约六秒游戏时间的送球动画。
- 实体键挡板、触摸挡板、蓄力与发球均有实际对应画面变化。
- 计分弹窗暂停时连续截图逐像素一致，关闭后恢复。
- 顶部触摸打开计分板、Esc 关闭；在球台按住 Esc 保持，抬起后返回固件菜单。
- 35 秒真实输入游玩得到 73750 分；两份 20 字节最高分记录均通过 magic、版本和 FNV 校验。

精简报告：[emulator-result.json](verification/emulator-result.json)。
截图在 [screenshots/](screenshots/)，与报告对应。

## 宿主测试与资源验证

原版引擎实际资源宿主测试：343 个组件、挡板与蓄力发球、20 秒游玩得分 32000；
暂停冻结时间和球位置，最高分保存读回与清理退出通过。
原有报告在 [original-host-results.json](verification/original-host-results.json)。
资源归档和 FILE 缓存测试覆盖一次原始读取、共享只读视图、借用内存所有权、seek/EOF、
保存文件绕过缓存、分配/短读回退和损坏归档拒绝。
本地导入的 66 个原版文件逐项符合清单；生成的 `PINBALL.PAK` 为 2179543 字节，SHA256
`42c8616ba5a5dcfc8499a2b5f87ca0280b5287279175c5eff620807314f02f4d`。

## 边界

完整模拟器测试只覆盖 A 盘；B 盘路径已实现，尚无 B 盘或 9588 真机实测。
音频管线有输出记录，未人工试听。未将模拟器视为真机性能测量。
GitHub Actions 尚未运行；配置中的构建、静态校验和缓存测试不等于模拟器验证。
原版图标再分发授权仍待明确，见来源声明。
