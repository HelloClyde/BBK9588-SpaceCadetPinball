# 原版游戏资源

`PINBALL.PAK` 是固定的原版资源归档，含球台 DAT、位图、字体、60 个 WAV 等全部 66 个文件。
每个文件的大小、SHA-256、来源提交以及整个归档的校验信息记录在 `original-resources.json`。

归档大小：2179543 字节。
SHA-256：`42c8616ba5a5dcfc8499a2b5f87ca0280b5287279175c5eff620807314f02f4d`。

维护者于 2026-10-28 确认具有对应分发授权，要求将资源随完整安装 ZIP 提供。
资源保留原权利归属，不按应用代码的 MIT 许可证重新许可，详见 [来源声明](../SOURCE_NOTICE.md)。

普通构建与 CI 直接校验并打包此归档，不依赖临时资源下载：

```powershell
python resource_archive.py
python package_release.py
python scripts/verify_install_zip.py dist
```

需要再生成时，在仓库根目录运行：

```powershell
python fetch_original_data.py
python make_resource_pack.py
```

或先用 `python import_original_data.py '原版文件目录'` 导入符合固定清单的本地文件，再运行
`python make_resource_pack.py`。输出为 `original-data/PINBALL.PAK`，应与本目录归档逐字节一致。
提取脚本只解析固定来源的 base64 数据，不执行下载脚本。
