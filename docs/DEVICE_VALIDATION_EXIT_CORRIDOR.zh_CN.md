[English](DEVICE_VALIDATION_EXIT_CORRIDOR.md) | 简体中文

# 走廊原型设备验证

状态：2026-09-20 已安装固件。当前物理设备运行经验证的 2,312,240 字节纯色墙面版本（写入 `0x10000`）。开机启动已确认，长时游玩手感及最新拐角明暗对比度优化仍待真机验证。

## 当前安装状态

- **已安装固件**：2026-09-20 纯色墙面应用（2,312,240 字节）写入 ESP32-C3 的 `0x10000` 分区。分区表与合并镜像一致，原 3 MB 应用已在本地备份。引导扇区、分区表、NVS、身份信息及永久 Recovery 均未更改。
- **硬件验证结果**：设备正常重启。串口截图确认开机菜单中成功选中 Corridor（[菜单截图](../assets/images/exit-corridor/device-validation/firmware-plain-wall-menu-20260920.png)）。[安装元数据与写入哈希](../assets/images/exit-corridor/device-validation/firmware-install-plain-wall-20260920.json)。
- **拐角明暗对比度优化**：横向连接通道墙面（`side >= 2`）暗一级，保留纵深折线与转弯辨识度。64 组同视点接缝与双外拐角对比度测试通过；原生与 Wasm 像素级一致。**当前状态：已在主机测试与 Wasm 网页预览中验证，尚未刷写到物理设备。**

## 历史性能实测数据

在连接的 ESP32-C3（修订版 1.1，8 MB Flash，无 PSRAM）上记录的各阶段实测数据：

| 固件版本 / 试验方案 | 提交帧率 (FPS) | CPU 渲染耗时 | LVGL 刷新耗时 | 内部空闲堆 (最低) | 结论 / 状态 |
| --- | --- | --- | --- | --- | --- |
| **初始可玩版本** | 约 4.0 | 105–118 ms | 未分段 | - | 基线版本 |
| **内存修复（1位遮罩）** | 4.2–4.7 | 104–126 ms | - | 15,620 B (11,180 B) | 解决连续内存分配失败 |
| **定点射线/告示，单 20 行缓冲** | 6.7–7.1 | 52–59 ms | 约 85 ms | - | 圆弧拐角中间版 |
| **墙砖查表，双 10 行缓冲** | 6.5–6.8 | 场景相关 | 约 96 ms | - | 舍弃（帧率下降） |
| **按需截图流，双 40 行缓冲** | 10.5–11.4 | 46–53 ms | 约 38 ms | 27,500 B (23,060 B) | 保留（释放 40,800 B 截图缓冲） |
| **定点步进设置** | 11.0–12.1 | 42–49 ms | 约 38 ms | 27,500 B (23,060 B) | 纯色墙面前的历史实测基准；保留该定点设置 |
| **对中测试自动探针** | 12.3–13.0 | 36–39 ms | 约 38 ms | - | 临时按键探针，已从交付源码移除 |
| **生产实走（55秒采样）** | 10.7–12.7 | 37–51 ms | 约 38 ms | 27,572 B (23,060 B) | 移除自动探针后的干净版本 |

历史运行日志样本：[生产实走样本](../assets/images/exit-corridor/device-validation/firmware-production-live-20260920.json)、[固定表面样本](../assets/images/exit-corridor/device-validation/firmware-fixed-surfaces-20260920.json)。历史运行时样本中，内部空闲堆记录为 27,500～27,572 B（最低 23,060 B），最大连续块 14,848 B；未出现 panic、看门狗或内存耗尽标记。帧窗口统计的是提交帧率，不包含完成 LCD DMA 的耗时或长时帧时间分位数。

## 历史 7 归 0 判定调查

早期试玩中，用户曾反馈出现 7 归 0 的情况。根据游戏规则，任何一次错误的边界跨越都会将得分清零；只有连续八次正确抉择才能通关。

由于当时运行的版本未记录单次异常和出口离开事件，该历史事件属于**不可归因**（无法确认是玩家选择失误还是代码逻辑缺陷）。后续诊断版本在串口添加了边界判定日志（`log_judgement`），同时保持盲玩界面不泄露答案。新增的 36 组测试覆盖了得分为 7 时的全部 9 种场景、双向进入和全部抉择路径；正确选择进 8，错误选择归 0。

## 已确认的通关显示缺陷（2026-09-20）

用户连接了仍显示「出口 7／行走中」的设备。被动日志记录 `before=7 after=8 correct=1 phase=2`，且 `frames=0 refreshes=0`；[屏幕](../assets/images/exit-corridor/device-validation/cleared-stale-hud-20260920.png)及[脱敏记录](../assets/images/exit-corridor/device-validation/cleared-stale-hud-20260920.json)证实本次已正确通关，但 HUD 未刷新，并非选择错误。采集过程中没有重启或发送按键。固件进入 `EC_CLEARED` 后停止重绘，却未标记画面需要更新；下一次 OK 因此会在仍显示游戏中的画面后直接重开。修复在状态切换时保留重绘请求，并跨越帧率限制。修复尚未刷写或通过真机验收。新结尾已在持续保留的通关提示页之前加入可行走的楼梯，见[网页验收](WEB_PREVIEW_EXIT_CORRIDOR.zh_CN.md)。此前没有日志的事件仍不可归因。

## 可重复的主机截图采集

主机截图工具（`tools/capture_passport_screen.py`，经 `tests/test_capture_passport_screen.py` 测试）使用 `FAP_SCREENSHOT_V1` 协议。在持有 LVGL 锁时按需流式传输 120 × 160 RGB565LE 行，移除了原有的 40,800 B 常驻截图缓冲，为 LCD 双 DMA 缓冲腾出了宝贵内存。

```bash
PYTHONDONTWRITEBYTECODE=1 python3 tools/capture_passport_screen.py   --port /dev/cu.usbmodem2101 --timeout 10 --output /tmp/passport-capture-01
```

每次截图输出 PNG、原始载荷 SHA-256 哈希及带时间戳的 JSON 元数据。验证证据：[初始菜单截图](../assets/images/exit-corridor/device-validation/menu-20260919.png)（[元数据](../assets/images/exit-corridor/device-validation/menu-20260919.json)）、[复用工具采集元数据](../assets/images/exit-corridor/device-validation/menu-tool-20260919.json)及[纯色墙面菜单截图](../assets/images/exit-corridor/device-validation/firmware-plain-wall-menu-20260920.png)。

## 剩余待验证边界

- **物理操作手感**：自动行走圆弧过弯（0.85 米半径）和观察位对齐（斜向物件最远 1.2 米）的用户舒适度仍待实测评价。
- **长时稳定 >=12 FPS**：物理 ESP32-C3 芯片在长时连续游玩下的稳定 >=12 FPS 帧率仍未测定。
- **拐角明暗对比度真机刷写**：最新的横向墙面（`side >= 2`）暗一级优化已在主机和 Wasm 中验证，但尚未刷入物理设备。
- **实体三键完整通关**：使用实体三键从 0 到 8 的完整通关实机游玩仍待实测。
