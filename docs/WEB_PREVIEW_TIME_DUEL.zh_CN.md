[English](WEB_PREVIEW_TIME_DUEL.md) | 简体中文

# 掐秒挑战固件网页预览

交互式[网页预览](http://127.0.0.1:8098/time-duel-v2.html)通过 WebAssembly 运行固件游戏状态机。请先启动下文的本地服务；直接用 `file://` 打开无法加载 WebAssembly 模块与素材。每个游戏使用独立的 Demo 网页。

## 共用代码与架构

- `main/duel_clock.c` 和 `main/duel_clock.h` 原样编译为 `prototype/time-duel/firmware/duel.wasm`。纯 C 状态机控制回合阶段（`HOME`、`TARGET`、`TIMING`、`HANDOVER`、`SEALED`、`ROUND_END`、`RESULT`、`MATCH_WIN`）、目标时长抽取（1.0～6.0 秒，步进 0.5 秒）、估时计算、胜负判定（`abs(估时 - 目标)`）、五局三胜比分记录、轮换先手机制以及带种子的 AI 高斯对手估时（`sigma = 0.12 * 目标`）。
- `tools/duel_web_bridge.c` 中的薄层 C 桥接暴露输入处理（`web_key`、`web_handle`）、时间步进（`web_tick`）、生命周期控制（`web_init`、`web_home`、`web_exit`）、状态查询（`web_state`）与确定性评审样例设置（`web_set_fixture`）。
- `prototype/time-duel/firmware/runtime.mjs` 负责 WebAssembly 模块加载、导出函数绑定及浏览器集成状态解码。
- `prototype/time-duel-v2.html` 网页外壳为 `main/duel_ui.c` 中的 LVGL 页面布局提供 240×320 浏览器视觉参考，沿用《合金弹头》风格的军事像素美术（红头带与蓝贝雷帽特工、夕阳前哨、军绿外壳与沙金字色）；原生 C 与 WebAssembly 测试仅确立模型状态一致性，不代表视觉渲染等价。
- `prototype/time-duel-audio.js` 通过 Web Audio API 合成原创芯片背景音乐及六种提示音效，严格遵循估时计时阶段静音规则。
- `prototype/time-duel/firmware/manifest.json` 记录所需原生时钟源码与生成的 `duel.wasm` 的 SHA-256 校验和，页面加载时自动核验模块完整性。

网页层作为共用 C 掐秒引擎的外层视觉与音频呈现壳，不再重复实现独立的游戏状态机循环。

## 运行与重建

本地预览检查需要 Python 3.10+ 与 Node.js 18+。查看已构建的预览仅需现代浏览器及本地 HTTP 服务，无需嵌入式工具链。

```bash
python3 tools/serve_corridor_web.py --port 8098
# 打开 http://127.0.0.1:8098/time-duel-v2.html
python3 tools/build_duel_web.py --check
node tests/test_duel_web.mjs
node tests/test_duel_preview.mjs
```

服务严格绑定回环地址，仅挂载预览与素材目录。

修改原生时钟源码或网页桥接后，使用官方 [WASI SDK](https://github.com/WebAssembly/wasi-sdk/releases) 重建（已验证 34.0）：

```bash
python3 tools/build_duel_web.py --sdk /path/to/wasi-sdk
./tools/validate.sh --static
./tools/validate-games.sh
# 激活 ESP-IDF 5.5.3 环境后运行完整门禁：
./tools/validate.sh
```

将 `duel.wasm`、`runtime.mjs` 与 `manifest.json` 统一保存在 `prototype/time-duel/firmware/` 目录下。该目录同时包含 4 份 WASI 运行时许可证说明文件（`COMPILER_RT_LICENSE.txt`、`MUSL_COPYRIGHT.txt`、`WASI_LIBC_LICENSE.txt`、`WASI_MIT_LICENSE.txt`）。素材来源与说明见[资源清单](../assets/README.zh_CN.md#time-duel-美术素材)。

## 操作与导航

在《掐秒挑战》网页预览中：
- **短按 OK**（空格／Enter／点击屏幕按键，按下即触发）：在按键按下时立即触发游戏阶段动作（避免误导为松手才计时），包括首页开始比赛、开始与停止掐秒、确认封存成绩、跳过庆祝动画以及进入下一回合。
- **长按 OK 1 秒**：取消本场比赛，返回当前 Demo 的游戏首页。
- **DOWN 键**：在首页开关芯片音乐与提示音效。
- **UP 键**：游戏中未启用；在首页切换双人对抗与 AI 练习模式。

### 固件开机与导航流程

统一固件入口 `main/game_main.c` 整合全部游戏：
1. 固件默认直开《8号出口》（Corridor）。
2. 在《8号出口》游戏中，长按 OK 键一秒退回走廊标题页。
3. 在走廊标题页长按 OK 键打开游戏选择页（`main/game_launcher.c`）。
4. 在游戏选择页中，按 UP／DOWN 在《8号出口》与《掐秒挑战》之间切换，按 OK 进入选中游戏。
5. 在《掐秒挑战》中，长按 OK 键一秒直接退回游戏选择页。
6. 网页 Demo 各自独立；长按 OK 返回当前游戏首页，不模拟固件层的游戏选择。

### 轮流传递对决流程

- **交接页**：复用 40 px 字号的目标时长卡片，隐藏先手估时成绩，后手按一次 OK 即可立即开始计时，无需二次确认。
- **成绩封存**：双方完成前成绩完全保密，直到单独按 OK 确认后才揭晓胜负。
- **庆祝与结算**：1.5 秒独立回合庆祝动画与盖章横幅，详细数值成绩页停留等待 OK 确认，3.5 秒整场胜利展示后自动返回首页。

## 验证与边界

网页预览仅供视觉、音效与交互流程评估，浏览器运行表现不能作为物理硬件设备的验收凭据。测试套件验证原生 C 与 WebAssembly 的状态一致性，而非硬件手感或物理计时精度。

后端测试套件（`node tests/test_duel_web.mjs`）通过原生 C 与 Wasm 之间的确定性轨迹对齐测试（39 个快照、95 条指令、状态差值为零）。2026-09-27 的浏览器冒烟检查覆盖当时的选择页导航、双人超时与封存确认、详细成绩、返回首页及 AI 模式切换／开局，未见控制台错误。完整盲玩验收与真机验证仍待完成；这些检查不代表 LVGL 渲染等价。

相较于真实物理硬件的核心差异：
- **电量**：网页预览使用示意 `--%` 电量显示；真机通过 ADC 读取物理电池电压。
- **计时与按键输入**：网页端使用浏览器高精度时间戳（`performance.now()`）。真机由 BSP 通过 `esp_timer` 定时任务轮询采样电阻分压网络，并在按键事件中捕获 `esp_timer_get_time()` 微秒时间戳。
- **音频**：浏览器通过 Web Audio API 合成音频波形；真机通过 I2S 将 16 kHz 单声道 PCM 流输出至 ES8311 Codec 芯片。
- **硬件与分区约束**：浏览器不模拟 ESP32-C3 硬件限制（无 PSRAM）、本分支配置的 3 MB 应用分区边界、SPI／DMA 屏幕总线时序、电阻分压按键去抖及低功耗电源管理。
- **固件安全**：网页构建过程不修改也无法刷写物理设备分区。

`node tests/test_duel_preview.mjs` 使用真实 Wasm 核心与受控时钟验证网页输入适配层，覆盖按下／松开与重复输入、封存成绩、模式切换、输入中断及长按返回首页。这些检查不测量实体按键延迟。

## Fork 游戏验证

`tools/validate.sh` 负责通用仓库/BSP 检查和固件打包，不调用游戏测试。
另外运行 `./tools/validate-games.sh`，验证两款游戏的原生逻辑、渲染、声音、
生命周期、启动器和 C/Wasm 一致性。存在游戏专用资源场景时，将它们放在此应用门禁；
可复用的资源模型保持独立，不依赖任何游戏。

游戏门禁需要 Node.js 18+、Python 3.10+ 和 C11 编译器；检查已有 Wasm 产物不需要
WASI SDK。`.github/workflows/game-checks.yml` 在 PR 或推送到 `main` 涉及应用代码、
共享 BSP、素材、预览、测试、工具、构建配置或该工作流时运行相同命令，也支持手动触发。
纯文档变更会跳过，因此不要将这个按路径触发的状态设为无关变更的必需检查。
游戏发布需要同时通过通用门禁和游戏门禁；两者都不能代替真机验收。
