[English](TASKS_EXIT_CORRIDOR.md) | 简体中文

# 走廊原型开发任务与验收

**当前入口（EC-32）**：网页端（[`prototype/exit-corridor.html`](../prototype/exit-corridor.html)）现已直接运行编译为 WebAssembly 的固件 C 代码与素材，见[同步与重建说明](WEB_PREVIEW_EXIT_CORRIDOR.zh_CN.md)。早期 JavaScript 原型运行时已停用归档；角色烘焙工具与帧包校验测试（`packed-sprites.js` / `.test.cjs`）继续保留。

开发基线为 `feature/exit-corridor` 分支，起点为本地 `main` 的 `2e6813b`。核心调研依据与设计边界见[走廊玩法研究](RESEARCH_EXIT_CORRIDOR.zh_CN.md)。

## 已交付能力

| 能力 / Ticket | 实现摘要 | 验证结果 |
| --- | --- | --- |
| **原作依据与独立边界** (EC-01) | 引用一手权威来源明确与《8号出口》的独立致敬边界。 | 文档已记录并确认。 |
| **游戏规则与连贯性** (EC-02, EC-24) | 连续正确八次通关、走错归零；按进入侧判断前进与折返；无缝连续连接通道。 | 主机测试套件通过：跨界判定、按进入侧判定、无缝连续转弯、3600 帧 NPC 运动及 36 组终局边界用例。 |
| **原创异常设计** (EC-03) | 八种可逆原创异常（缺门、海报眼睛、倒挂海报、额外通风口、红灯、高大路人、停止凝视、路人缺席）；基准场景稳定。 | 全部 8 种异常在原生 240 × 320 完成巡检；5 组快捷情景 1,2,2,0,8 通过。 |
| **操作与导引** (EC-04, EC-14) | 唯一实体三键契约：上下键按下转向 45°、OK 短按在松开时走停、长按 1 秒退出。 | 输入状态机、失焦与可见性处理通过。 |
| **NPC 连续步态动画** (EC-08, EC-11, EC-18) | MIT Rocketbox 离线烘焙：8 方向 × 16 步态 + 1 站姿（136 帧，312,689 B 帧包）。C 渲染器按需仅将当前活跃帧解码至 4,608 B + 576 B 遮罩。 | `packed-sprites.test.cjs` 确定性编码与往返解码全部通过。 |
| **2.5D 软件光线投射器** (EC-16, EC-22) | 整型光线投射、浅灰纯色墙面、纵深光照、踢脚线与精灵投影，无墙砖查表或 PNG 图集开销。 | 8241 像素无物件纯色墙面在 9 处视角完全稳定通过。 |
| **显示与内存架构** (EC-15, EC-29) | 原生 240 × 320 竖屏输出。两块 40 行 DMA 双缓冲（共 38,400 B），由按需串口行流式传输（`FAP_SCREENSHOT_V1`）替代原 40,800 B 全帧截图缓冲提供空间。 | 历史实走样本录得内部空闲堆 27,572 B（最低 23,060 B）。 |
| **圆弧拐角辅助** (EC-30) | 自动行走沿 0.85 米圆弧平滑过弯并正对下段通道停步。OK 暂停／继续；方向键立即覆盖。 | 主机拐角走入、偏移、暂停与手动覆盖测试全部通过。 |
| **观察位对齐辅助** (EC-31) | 正对侧墙时平滑对中附近标准物件（近处 <=0.65 米，斜向可见 <=1.2 米），限速 <=1.2 米/秒。正常与异常场景锚点完全一致。 | 162 组近处 + 36 组斜向可见用例及 ASan/UBSan 通过。 |
| **拐角明暗对比度** (EC-32) | 横向连接通道墙面（`side >= 2`）暗一级，保留纵深折线与转向方向辨识度，无需纹理查表。 | 64 组同视点接缝与双外拐角对比度测试通过。 |
| **WebAssembly 一致性** (EC-32) | 固件 C 游戏与渲染器原样通过 WASI SDK 编译为 `corridor.wasm`。原生 240 × 320 显示外壳。 | 1,343 条指令与 59 帧采样完全匹配原生，0 像素差异，状态误差 2.98e-8。 |
| **固件安装与验证** (EC-28) | 验证通过的 2,312,240 字节纯色墙面固件成功刷入物理 ESP32-C3 的 `0x10000` 分区。 | 正常启动、菜单选择及串口截图交互验证通过。 |

## 模块契约

活动入口：[`prototype/exit-corridor.html`](../prototype/exit-corridor.html)。

- `prototype/exit-corridor/firmware/app.mjs`：浏览器输入、主循环、开发评审界面绑定。
- `prototype/exit-corridor/firmware/display.mjs`：原生 240 × 320 RGB565 扫描输出量化及基于固件字模的文字绘制。
- `prototype/exit-corridor/firmware/input.mjs`：实体三键状态机（方向键按下生效、OK 松开短按动作、长按 1 秒退出）。
- `prototype/exit-corridor/firmware/runtime.mjs`：Wasm 实例生命周期管理、状态映射与 `manifest.json` 校验。
- `prototype/exit-corridor/firmware/bridge.c`：导出游戏和渲染器函数到 WebAssembly 的薄层 C 包装。
- `prototype/exit-corridor/firmware/corridor.wasm`：编译后的 C 语言游戏引擎与软件渲染器。
- `prototype/exit-corridor/firmware/presentation.json`：从固件提取的位图字体与 UI 文案。
- `prototype/exit-corridor/firmware/manifest.json`：源码与产物 SHA-256 完整性清单。
- `prototype/exit-corridor/firmware/style.css`：掌机外框响应式布局与界面样式。
- `main/corridor_game.{c,h}`：权威游戏逻辑、状态机、圆弧拐角辅助与观察对中。
- `main/corridor_render.{c,h}`：2.5D 软件射线投射、纯色墙面、拐角对比度与精灵投影。
- `main/demo_corridor.c`：ESP32-C3 硬件适配层与 LVGL 界面驱动。
- `prototype/exit-corridor/tools/pack-sprites.cjs`：生成 `commuter-device.bin` 的离线精灵编码工具。
- `prototype/exit-corridor/packed-sprites.js` / `packed-sprites.test.cjs`：保留的帧包格式校验测试。
- `prototype/exit-corridor/tools/bake.html`、`bake-server.py`、`test_bake.py`、`prototype/exit-corridor/vendor/`：离线角色烘焙工具链。

## 试玩与复现方法

使用 HTTP 服务监听回环地址，打开 `http://127.0.0.1:8098/`：

```bash
python3 tools/serve_corridor_web.py --port 8098
python3 tools/build_corridor_web.py --check
node tests/test_corridor_web.mjs
```

运行仓库静态检查与主机测试套件：

```bash
PYTHONDONTWRITEBYTECODE=1 ./tools/validate.sh --static
```

标题只写目标。四条通用规则位于入口左墙（$Z = -3.5$）；盲玩隐藏答案。开发评审提供指定异常样例与内部状态检查。

## 剩余验收清单

| 验收目标 | 当前状态 | 边界 |
| --- | --- | --- |
| 自动化主机与 Wasm 门禁 | 通过 | 完整静态检查与原生/Wasm 一致性验证通过。 |
| 纯色墙面真机固件安装 | 通过 | 2026-09-20 固件已安装至 `0x10000` 并验证启动。 |
| 拐角明暗对比度优化 | 主机/Wasm 通过；**真机待刷写** | 已在测试与 Wasm 预览中验证，尚未刷入实体设备。 |
| 用户盲测公平性 | 待验证 | 240 × 320 原生竖屏盲测体验待用户确认。 |
| 实体按键手感与握持 | 待验证 | 分压电阻长按转向时序与长时间握持手感待实测。 |
| 芯片持续稳定 >=12 FPS | 待验证 | 历史生产样本实测 10.7–12.7 提交帧/秒；长时持续基准待测。 |
| 实体按键完整 0→8 通关 | 待验证 | 纯实体三键从头到尾完整通关游玩待实测。 |
