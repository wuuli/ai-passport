<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

另见下方 [Time Duel 美术素材](#time-duel-美术素材)及其固件衍生资源。

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。
- 「掐秒挑战」在 `main/duel_sound.c` 中使用原创程序合成音乐及六种提示音，适用仓库代码许可，不导入录音或商业游戏配乐。worker 每块生成 128 个采样的 16 kHz、16 位单声道 PCM，无需音频转换或二进制素材；计时过程中禁用音乐。

## Time Duel 美术素材

- `images/time-duel/time-challenge-outpost.png`：1536 × 1024 不透明 RGB PNG，原创夕阳军事训练基地，包含无线电指挥车与掩体。
- `images/time-duel/time-challenge-operatives.png`：1254 × 1254 不透明 RGB PNG，深橄榄底色，2 × 2 角色图集。上排为待命，下排为举拳胜利姿态；左列为红头带特工，右列为蓝贝雷帽特工。
- `images/time-duel/time-challenge-operatives-alpha.png`：从角色图集可复现生成的 RGBA 衍生图。`tools/convert_duel_assets.py` 只将与图片边缘连通的底色 flood-fill 为透明像素，再裁切设备人物姿态。
- `images/time-duel/time-challenge-cover.png`：1086 × 1448、精确 3:4、不透明 RGB PNG；社区封面展示两名原创特工在夕阳前哨进行友好的掐秒比拼。以以上两张源图作为角色／风格参考，通过 OpenAI 内置生图工具生成，不含标志、文字、武器或第三方源素材。
- 集成：`prototype/time-duel-military.css` 通过仓库内相对路径为 `prototype/time-duel-v2.html` 加载场景；定位的人物层改用透明设备衍生图，让特工融入前哨背景，不再显示矩形底色。原始生成源图保持不变。
- 来源／许可：使用 OpenAI 内置生图工具为本原型与发布封面生成，并非从商业游戏提取素材；未附第三方源素材许可证。已保留[生成及最终编辑提示词](images/time-duel/generation-prompts.txt)，便于复现。
- 网页设备衍生素材：`images/time-duel/device/*.png` 由 `python3 tools/export_duel_preview_assets.py` 直接解码 `main/duel_assets.c` 中的 RGB565 背景与四组 RGB565A8 人物数组生成，仅依赖 Python 标准库。网页屏幕定位使用这些 240 × 240／80 × 112 图片，不再裁切遮罩原图集；评审外框仍使用原始美术。这些 PNG 不增加固件载荷。
- 固件衍生素材：`main/duel_assets.c` 包含一张 240 × 240 不透明 RGB565 背景及四张 80 × 112 透明 RGB565A8 待命／庆祝姿态，合计占 Flash 222,720 字节。大型源 PNG 本身不链接进固件。新增 Alpha 平面用于去除人物矩形底色，同时仍满足应用预算。
- 中文字形子集：`main/duel_font.c` 为 16 px 文案及 ASCII，`main/duel_title_font.c` 为 26 px 标题，使用 Noto Sans SC，[SIL OFL 1.1 许可证](fonts/time-duel/OFL.txt)。来源：[Noto Sans SC Regular](https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf)，SHA-256 为 `faa6c9df652116dde789d351359f3d7e5d2285a2b2a1f04a2d7244df706d5ea9`。完整字体只是构建输入，不是运行时依赖。
- 复现环境为 Python + Pillow 11.3.0、`lv_font_conv` 1.5.3：`python tools/convert_duel_assets.py --font /path/to/NotoSansSC-Regular.otf --font-converter /path/to/lv_font_conv`。保留源 PNG，从 UI 文案重新生成图像描述符和两套字体；新增中文文案后需重新运行。固件图像数据从 Flash 读取，峰值 RAM 与屏幕效果仍需真机验证。

## 出口走廊角色美术素材

- `images/exit-corridor/commuter-device.bin` / `.json`：默认 8 方向 × 16 步态 + 站立行，48×96 单元格，312,689 B（305.36 KiB），按需按帧解码。
- `images/exit-corridor/sprite-atlas-16.png` / `.json`：768×1632 离线源图（813,591 B），保留完整 16 视角用于烘焙和打包；旧 `sprite-atlas.png` / `.json` 保留作打包测试输入。
- `images/exit-corridor/ROCKETBOX_LICENSE.txt`：Microsoft Rocketbox MIT 许可（Copyright 2020 Microsoft），固定提交 `0943055db6ec570bcef9f2c8b41c9e5467c808f9`；Business_Male_01、neutral walk 和 idle 仅用于离线烘焙。
- 人物世界取景为 1×2 米、身高 1.76 米、脚底锚点 0.95。16 帧步态周期 1.067 秒；固件通过 `target_add_binary_data` 将 `commuter-device.bin` 直接链接进 Flash，由 C 渲染器按需仅解码当前活跃帧（`r->frame` 4,608 B + `r->alpha` 576 B）。
- 详细溯源、格式和可复现命令见[人物素材说明](images/exit-corridor/README.zh_CN.md)。

当前地下通道网页直接使用固件素材包与 C 渲染，生成字体数据来自 `main/corridor_font.c`；共享源码、重建与边界见[网页同步说明](../docs/WEB_PREVIEW_EXIT_CORRIDOR.zh_CN.md)。
