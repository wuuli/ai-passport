<p align="right"><strong>简体中文</strong> · <a href="ble-recovery-compatibility.md">English</a></p>

# 旧版固件布局引用

现行规范见[固件布局](firmware-layout.zh_CN.md)。3 MB 应用上限、`cardid` 和 Recovery 分区不再是上游模板的要求。此链接为下游旧文档保留。

本游戏分支为兼容已安装设备，明确保留现有 `partitions.csv` 和上键 Recovery 入口。入口钩子不会安装或验证 Recovery 应用。刷写前核对配置布局和实际设备；无需备份原固件。
