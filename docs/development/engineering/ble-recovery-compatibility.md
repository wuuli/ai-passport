<p align="right"><a href="ble-recovery-compatibility.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Legacy firmware-layout reference

The current policy is [Firmware layout](firmware-layout.md). A 3 MB application limit, `cardid`, and a recovery partition are not requirements of the upstream template. This link is retained for older downstream notes.

This game fork deliberately keeps its existing `partitions.csv` and UP-key recovery hook for compatibility with installed devices. The hook does not provision or validate a recovery application. Check the configured layout and actual device before flashing; no original-firmware backup is required.
