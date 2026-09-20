[English](WEB_PREVIEW_EXIT_CORRIDOR.md) | 简体中文

# 地下通道固件网页预览

主[预览](../prototype/exit-corridor.html)通过 WebAssembly 运行当前固件的游戏与渲染代码，替代独立 JavaScript 原型；早期 JavaScript 原型运行时已停用归档，角色烘焙工具与帧包校验测试继续保留。

## 共用代码与素材

- `main/corridor_game.c` 和 `main/corridor_render.c` 原样编译。三键行走、圆弧拐角、斜向可见物件对中、按进入侧判定、走错归零和连续八次通关均使用同一份 C 实现。
- 网页加载同一份 312,689 字节的 `commuter-device.bin`；调色板、人物查表和告示数据由固件 include 编入。墙面采用浅灰纯色，保留距离明暗和踢脚线，不使用墙砖查表或 PNG 图集。横向连接通道墙面（`side >= 2`）暗一级以保证拐角明暗对比。原生 C 与 Wasm 像素级一致性已通过；64 组同视点接缝与双外拐角对比回归通过。该拐角对比度优化已在测试与 Wasm 网页预览中验证，目前尚未刷写到物理设备（设备当前运行 2026-09-20 纯色墙面安装版）。
- 生成的 `presentation.json` 提取固件位图字体，并与 `demo_corridor.c` 校验标题／状态文字。Canvas 外壳按 RGB565 量化展开索引画面，以原生 240×320 绘制界面。
- `manifest.json` 保存源码与生成产物的 SHA-256；加载时核验 Wasm、界面数据和人物素材，静态门禁拒绝源码修改后未重建的旧产物。

薄层 C 桥接只暴露输入、推进、输出及显式评审样例。网页层处理指针／键盘、可见性、加载、显示和开发工具，不再独立实现游戏状态机。

## 运行与重建

本地预览检查需要 Python 3.10+、Node.js 18+ 和 C11 编译器。查看已生成的预览只需要现代浏览器及 HTTP 服务，无需 SDK。

```bash
python3 tools/serve_corridor_web.py --port 8098
# Open http://127.0.0.1:8098/
python3 tools/build_corridor_web.py --check
node tests/test_corridor_web.mjs
```

服务仅监听回环地址，只提供预览与素材子目录。端口已被占用时不要再启动第二个服务。

修改清单内的原生源码或素材后，使用官方 [WASI SDK](https://github.com/WebAssembly/wasi-sdk/releases) 重建（已验证 34.0）：

```bash
python3 tools/build_corridor_web.py --sdk /path/to/wasi-sdk
./tools/validate.sh --static
# Activate ESP-IDF 5.5.3 before the complete gate:
./tools/validate.sh
```

`corridor.wasm`、`presentation.json` 和 `manifest.json` 必须配套保存。运行库许可随模块保存在 `prototype/exit-corridor/firmware/`；人物来源见[素材说明](../assets/images/exit-corridor/README.zh_CN.md)。

## 操作与评审

UP／上箭头按下向左转 45°，DOWN／下箭头按下向右转 45°，短按 OK／空格／Enter 在松开时进入通道或切换走停。长按 OK 一秒返回网页标题；同时仅接受一个按键。失焦、隐藏标签页和取消触摸均停止行走。

开发评审默认收起；展开时暂停并显示答案。可选择全部九种正常／异常场景、用户反馈的 45° 海报位置、拐角及边界／最后一轮位置。继续观察恢复模拟，也可按 OK 开始行走。回到盲玩会从零开始，注入的样例不会带入盲玩。放大仅改变 CSS 显示尺寸。

## 验证与边界

2026-09-20 原生 C 与 Wasm 对照使用 1,343 条指令、59 个采样画面；4,531,200 个索引像素全部一致，状态最大误差为 2.98e-8。覆盖换轮、两个进入方向、最后一轮判定、转向、暂停和观察。这是采样等价检查，不代表所有可能操作都已穷尽。

网页交互验证确认：斜看海报转正后停在 X=0、Z=-20、90°；正常 7→8 通关及重玩；异常前进 7→0；异常折返 0→1。旧 WebGL 地址已加载共用固件预览。

网页不模拟 ESP32 内存压力、SPI／DMA 时序、实体 ADC 去抖、LVGL 合成、电量或设备菜单。电量为 `--`，长按 OK 回到预览标题。网页循环最多提交 20 帧／秒，性能显示明确标注非真机测量；原生帧缓冲对比不包含 Canvas HUD 合成。网页构建不修改或刷写设备固件。
