<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 出口走廊角色精灵表（Sprite Atlas）

当前网页与固件共用 `commuter-device.bin` 及 C 解码／渲染，见[网页同步说明](../../../docs/WEB_PREVIEW_EXIT_CORRIDOR.zh_CN.md)。早期 JavaScript 原型运行时已停用归档；角色源资产、离线烘焙工具与帧包校验测试继续保留。

本目录存放「出口走廊」通勤路人 NPC 的 2D 烘焙精灵图集、元数据及开源许可证文档。

## 文件与布局

- `exit-corridor-promo-cover-v6.png`：社区项目 562（`community-fb9b47f2`）当前使用的 3:4 封面。这是宣传插画，不是真机截图，固件和网页渲染器均不加载它。[编辑提示词](promo-cover-v6-prompt.txt)。
- `commuter-device.bin` / `.json`：默认运行时 ECSP 帧包及元数据；**312,689 B**，8 方向 × (16 行走 + 1 站立) = 136 帧。
- `sprite-atlas-16.png` / `.json`：离线 16 方向 × 16 步态源图，768×1632、272 单元格，PNG **813,591 B**；保留为离线主烘焙源图。
- `sprite-atlas.png` / `.json`：早期 8 方向 × 8 步态对照，384×864、72 单元格，PNG **219,522 B**；保留为历史对照源图。
- `ROCKETBOX_LICENSE.txt`：上游 MIT 许可原文。

单元格均为 48×96。方向 0 为正面，正角度从人物右侧观察。默认保留源图第 0、2、4…14 列（45° 步进），不镜像、不删步态；第 0–15 行为 1.067 秒的行走周期，第 16 行为站立姿态。完整对照的视角步进为 22.5°。

世界取景框为 1×2 米，人物实际身高 1.76 米，脚底锚点 0.95：框顶 Y=1.9 米、框底 Y=-0.1 米、脚底 Y=0。高大异常只沿 Y 拉伸。视角仍为离散采样。

## 资产出处与溯源

- **源项目仓库**：[Microsoft-Rocketbox](https://github.com/microsoft/Microsoft-Rocketbox)
- **固定提交哈希**：`0943055db6ec570bcef9f2c8b41c9e5467c808f9`
- **许可证**：MIT License（见 `ROCKETBOX_LICENSE.txt`），Copyright © 2020 Microsoft。
- **人物模型**：`Assets/Avatars/Professions/Business_Male_01/Export/Business_Male_01.fbx`
  - 运行时网格：`m005_hipoly_81_bones_opacity`（22,305 顶点，80 根运行时骨骼）。
  - 贴图资源：`m005_body_color.tga`（2048 × 2048）、`m005_head_color.tga`（2048 × 2048）、`m005_opacity_color.tga`（1024 × 1024）。
- **行走动画**：`Assets/Animations/all_animations_max_motextr_xy/m_walk_neutral.max.fbx`
  - 移除根位移：将 `Bip01.position` 轨道上的前向位移归零（$X=0, Z=0$），保留 $Y$ 轴自然的骨盆上下起伏。
- **站立姿态**：`Assets/Animations/all_animations_max_motextr_static/m_idle_neutral_01.max.fbx`
  - 在 $t = 0.5\text{s}$ 处采样真实休息站姿，双臂自然下垂、双脚着地（非从行走中途硬截取的半抬腿姿态）。

## 存储与内存

固件从 Flash 读取 ECSP，每次只将当前帧解码到 4,608 B 索引像素与 576 B Alpha 遮罩，不展开整张源图集。WebAssembly 预览使用相同 C 解码器；JavaScript 解码器只保留作离线格式测试。

选择八个视角并保留全部十六个步态采样，将人物包从 622,522 B 缩减为 312,689 B。FBX、骨骼和 PNG 解码仅用于离线烘焙。早期浏览器纹理总预算已退役；当前资源核算见[设计记录](../../../docs/RESEARCH_EXIT_CORRIDOR.zh_CN.md)。

ECSP 包含 24 字节头、绝对帧偏移、每帧包围盒、RGB565+Alpha 调色板与透明扫描行段。大于 8 的 Alpha 保留，0–8 舍弃。每帧超过 256 色时拒绝编码。损坏的头部、帧边界、调色板索引及截断流在试玩前明确报错。

## 复现步骤

1. 下载 Rocketbox 源资产至 `/tmp/exit-corridor-character-source/`：
   ```bash
   mkdir -p /tmp/exit-corridor-character-source
   BASE="https://raw.githubusercontent.com/microsoft/Microsoft-Rocketbox/0943055db6ec570bcef9f2c8b41c9e5467c808f9"
   curl -sL "$BASE/Assets/Avatars/Professions/Business_Male_01/Export/Business_Male_01.fbx" -o /tmp/exit-corridor-character-source/Business_Male_01.fbx
   curl -sL "$BASE/Assets/Animations/all_animations_max_motextr_xy/m_walk_neutral.max.fbx" -o /tmp/exit-corridor-character-source/m_walk_neutral.max.fbx
   curl -sL "$BASE/Assets/Animations/all_animations_max_motextr_static/m_idle_neutral_01.max.fbx" -o /tmp/exit-corridor-character-source/m_idle_neutral_01.max.fbx
   curl -sL "$BASE/Assets/Avatars/Professions/Business_Male_01/Textures/m005_body_color.tga" -o /tmp/exit-corridor-character-source/m005_body_color.tga
   curl -sL "$BASE/Assets/Avatars/Professions/Business_Male_01/Textures/m005_head_color.tga" -o /tmp/exit-corridor-character-source/m005_head_color.tga
   curl -sL "$BASE/Assets/Avatars/Professions/Business_Male_01/Textures/m005_opacity_color.tga" -o /tmp/exit-corridor-character-source/m005_opacity_color.tga
   ```
2. 启动本地烘焙服务：
   ```bash
   python3 prototype/exit-corridor/tools/bake-server.py
   ```
3. 在支持 WebGL 的浏览器中打开 `http://127.0.0.1:8099/tools/bake.html`。
4. 选择 **candidate_16x16**，点击 **Bake Sprite Atlas** 渲染 272 帧；shipped_8x8 仍可用，写入独立文件。
5. 点击 **保存到 Assets (Save)** 将图集与元数据写入 `assets/images/exit-corridor/`。

6. 从保存的源图生成设备帧包及其独立元数据：

```bash
node prototype/exit-corridor/tools/pack-sprites.cjs \
  assets/images/exit-corridor/sprite-atlas-16.png \
  assets/images/exit-corridor/sprite-atlas-16.json \
  assets/images/exit-corridor/commuter-device.bin \
  --dir-step 2 --metadata-out assets/images/exit-corridor/commuter-device.json
node prototype/exit-corridor/packed-sprites.test.cjs
```

`device-validation/menu-20260919.png` 是用户连接设备的只读串口截图解码，配对 JSON 记录原始 RGB565 载荷哈希；仅为验证证据，不加入游戏素材包。

`device-validation/menu-tool-20260919.json` 及配对 PNG 保存可复用主机工具的再次采集结果与 UTC 时间戳，载荷哈希与原始传输一致。`serial-observation-20260919.json` 仅记录 30.15 秒静默观察窗口，不证明稳定性。

`browser-validation/surfaces-20260919.json` 记录真实 Canvas 纹理转换：14 种表面、全部出口号 0–8、最大编码数据 103,512 B、最大常驻类型化数组 107,272 B。属于浏览器证据，不含未来固件容器，不是真机内存验证。

`browser-validation/flow-20260919.json` 记录真实浏览器输入、跨界行走、评审恢复、八段通关、重玩与回归结果。受控网页测试与用户盲玩、真机验收分别记录。

`browser-validation/seams-20260919.json` 记录两种渲染器真实走入前后拐角、保留朝向、按进入侧判定、0→8 恢复与重玩。包含源码哈希及已修复的 WebGL 初始化错误，仅属浏览器证据。

无感走廊衔接验收：[浏览器记录](browser-validation/seamless-20260919.json)，包含实际 GPU 像素对比、物理过廊与源码哈希。该记录替代此前的淡黑转场验收，不代表设备性能验证。

拐角辅助及原生横竖屏比较：[网页记录](browser-validation/corner-orientation-20260919.json)，包含 384 项衔接检查与实际拐角行走；真机性能及握持尚未验证。

固件将 `commuter-device.bin` 直接嵌入应用 Flash。`main/corridor_palette.inc` 保存专用 256 色调色板与光照表；原生画布含调色板为 77,824 B，64 位主机上的渲染工作区为 13,264 B（分块分配，含 576 B 位透明遮罩；设备实际结构大小由运行日志报告）。`main/corridor_font.c` 与 `main/corridor_notice.inc` 从 Noto Sans SC 裁剪，遵循 [SIL OFL](../../fonts/time-duel/OFL.txt)。通过 `tools/generate_corridor_font.py`、`tools/generate_corridor_notice.py`（Pillow）和 `tools/generate_corridor_palette.py` 复现；`tools/generate_corridor_sprite_lut.c` 生成仅占 Flash 的 65,536 B RGB565 转索引表。五行 RGB565 输出缓冲及其调色板另占 2,912 B 静态内存。字体与转换器使用素材总索引记录的 Noto Sans SC／lv_font_conv 1.5.3。固件调色板及程序化表面移植与 WebGL 参考效果有区别。

墙面采用浅灰纯色并保留纵深光照与踢脚线，横向连接通道墙面（`side >= 2`）暗一级以保证拐角明暗对比。真机圆弧转弯、观察对中、帧率与截图恢复证据见[设备验证记录](../../../docs/DEVICE_VALIDATION_EXIT_CORRIDOR.zh_CN.md)。
