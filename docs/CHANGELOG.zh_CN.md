<p align="right">
  <strong>简体中文</strong> · <a href="CHANGELOG.md">English</a>
</p>

# Changelog

## Unreleased

- 电量计就绪时，8 号出口标题页从首帧显示实测电量；若读数稍后才到，标签会自动更新。读取失败时暂显 `--`，直到后台取得有效读数。

- 8 号出口社区固件开机直接显示游戏标题页，不再经过通用 BSP 菜单。游玩中长按 OK 返回标题；仅当游戏渲染器分配失败时，才退回诊断菜单。

- 将 8 号出口网页预览文档指向 HTTP 入口；直接以 `file://` 打开页面时显示本地服务启动提示，避免模块与素材被浏览器拦截后一直停在加载画面。

- 修正走廊顶部门楣及两种通关楼梯方向的 EXIT 指示牌：从相反方向看时，使用朝向玩家的另一面文字，不再出现镜像反字。固件渲染器与网页预览共用这一修正。

- 8 号出口跨过判断边界后，左上角出口数字会等相机转入下一段走廊再更新，避免画面还没看到新出口牌时提前透露结果。判定与场景内编号仍在跨界时生效。

- 在 8 号出口开场画面加入简明操作说明，按真机位置标明顶部键左转、中部键右转、底部 OK 键控制走停；拐角自动转向并停步，再按 OK 继续。真机固件与网页预览使用相同文案。

- 优化 8 号出口的 240x320 开场与通关界面：新增仅四个字形的 30px 标题字体，采用暗色入口与日光色通关画面，分开主操作和底部返回提示，将循环呼吸改为一次轻柔渐显。网页与固件共用字体位图和文案；楼梯、白色出口、玩法及重玩规则保持不变。

- 将 8 号出口楼梯扶手改为靠墙安装的连续金属圆管，顺楼梯坡度延伸、上下平台处短距离水平收头，并由稀疏墙面支架承托，对齐原版《8号出口》楼梯间，替代原先每级台阶一根的密集竖栏杆。顶端白色天光与「EXIT 8」指示牌保持不变。

- 将「地下通道」入口改为游戏内标题页：画面保留背后的走廊与低压暗色遮罩，标题显示「8号出口」，按 OK 进入的提示轻柔渐显。网页预览与 LVGL 固件页面使用一致的标题／通关布局。

- 修复自动拐弯中途手动接管后朝向永久偏离的问题：左右键恢复到 45° 朝向网格，保留相机缓动和当前位置，避免后续拐角辅助与侧墙观察对中失效。

- 修复取消未完成的拐弯后继续行走会错误跳过辅助的问题。只有已完成的拐角才跳过；转回进入方向后，同一拐角可以再次引导玩家，无需重置位置。

- 在通关面板前增加可行走的 8 号出口终段：第八次正确判断后进入带台阶和自然光出口的通道，继续使用三键行走、暂停与观察。走到出口才正式通关；途中固定保留进度 8，通关后另按 OK 才重开。网页直接运行同一套 C 实现；尚待真机验收。

- 修复「地下通道」第八次正确判断后通关界面漏刷新：状态切换会保留最后一次 HUD 重绘请求，即使游戏已停止或当前帧被限频。此前画面可能停在出口 7，再按 OK 却直接重开。计分和判断错误归零规则不变。

- 新增「地下通道」3D 观察真机固件页面与开机默认选项：提供三键交互（左转、右转、走／停）、连续八次正确通行逃生机制、八种原创异常、按进入侧判断的折返规则，以及全中文 HUD、标题与入口告示。原有各项 Demo、掐秒挑战与永久 Recovery 保持完整。

- 实现面向无 FPU ESP32-C3 的原生 240×320 软件光线投射渲染器：采用定点射线、招牌投影与表面步进，消除大量软件浮点开销；采用浅灰纯色墙面结合垂直端墙固定明暗微调，在不引入细密格线抖动的前提下清晰呈现拐角折线；Flash 内嵌 Rocketbox 路人素材，C 渲染器按需仅解码当前活跃帧。

- 引入拐角平滑走位与侧墙观察位对齐辅助：自动行走沿 0.85 米圆弧平滑过弯并正对下段通道停步，支持 OK 暂停／继续及任意方向键立即覆盖；正对侧墙时平滑对中附近标准物件位置（含 45° 斜向可见、最大 1.2 米微调），限速运行且正常与异常场景锚点完全一致，绝不剧透答案。

- 升级显示管线并重构为按需截图流：移除原来常驻的 40,800 B 截图全帧缓存，改为仅在请求时按需逐行串口流式发送（`FAP_SCREENSHOT_V1`）；释放出的内部 RAM 支持两块 40 行 LCD DMA 缓冲（共 38,400 B），实现渲染与 SPI 发送重叠。5 行分块的 I8 转 RGB565 LVGL 流式解码、位压缩 Alpha 遮罩及内存不足降级保护降低分配压力；音频共存和长时间游玩仍需真机验证。

- 建立 WebAssembly 同源网页预览与自动化测试门禁：网页端直接编译并运行相同 C 游戏逻辑与软件渲染器，具备源码与产物一致性校验；主机测试覆盖观察几何位姿、圆弧拐角、进出通道判定（含 7→8 通关与 7→0 重置区分）、64 组跨通道传送接缝无缝比对及渲染器内存越界防护。

- 将《掐秒挑战》的目标范围从 1.0～3.0 秒扩展到 1.0～6.0 秒，保持 0.5 秒间隔及“目标 + 3 秒”自动停止规则。固件、网页自由试玩、旧原型数据、产品需求、分享文案和范围回归测试现统一使用 11 档目标。

- 将网页评审页与已完成的同机双人体验对齐：移除 AI 练习选择器及相关文案，将模拟器固定为轮流挑战，标记 UP 在本游戏中不使用，以明确标注的预览电量替代持续显示的不可用占位，并修复模式清理后超时、回合动画和整场动画的自动推进。声音、交接一次按键开始、成绩封存、回合庆祝、结果停留及整场返回首页均与已试玩的双人流程一致。

- 去除《掐秒挑战》人物在固件与网页预览中的矩形底色。可复现素材管线会将源图中与边缘连通的底色转换为透明区域，人物姿态改用 RGB565A8，背景仍为 RGB565，并新增描述符格式、字节数与 Alpha 平面的回归检查。

- 为《掐秒挑战》补齐社区发布与直接分享能力：新增只读 `FAP_SCREENSHOT_V1` 串口画面服务、支持分段输入的协议匹配主机测试、精确 3:4 的生成式封面，以及双语发布／安装指南。服务从真实 LVGL 刷新流维护 120 × 160 验证画面，避免在无 PSRAM 设备上分配整屏 DRAM；它不会刷机、重启或改变游戏状态。

- 将已评审的交接页布局同步到固件：交接时去掉两名特工与 VS，复用目标页 192 × 105 的时长卡片及 40 px 数字。保留一次按键计时、先手成绩隐藏、超时提示和独立电量刷新；新增覆盖全部目标档位、双方玩家、正常／超时交接的卡片像素一致性回归，同步网页状态与需求说明。

- 修复「掐秒挑战」首次进入时电量百分比不更新：LVGL tick 独立检查 worker 电量缓存，不再依赖游戏状态切换；仅在归一化电量变化时更新右上角标签。延迟读数、不可用／恢复及退出重入无需按键或整屏重绘。新增主机回归及真实 LVGL 检查，确认游戏区像素、动画进度不受影响，退出后不再访问 UI。电量 I/O 仍在 worker 执行，冷启动实机验收仍待完成。

- 调整网页交接页供评审：移除双人 VS 画面，复用目标页完整的大号时长卡片，保留一次按键直接开始及先手成绩封存；后续固件实现记录见前文。

- 将网页交互预览同步到当前固件：使用设备衍生像素素材与屏幕布局，交接一次按键直接计时，封存确认后才加分，独立 1.5 秒回合动画，具体成绩停留等待，整场胜利后回首页。新增可暂停的九步画面导航、五组评审情景、相同的合成音乐／提示音及网页纯逻辑测试。明确浏览器字体、声音、计时、电量和示意菜单不属于实机证据。

- 明确「掐秒挑战」结算顺序：双方完成后封存成绩、保留旧比分，额外按 OK 后才播放小回合胜利动画；动画结束自动进入具体数值，下一回合仍需 OK 确认。仅将背景音乐幅度提高 40%（约 2.9 dB），保持 75/100 输出音量和六种提示音波形不变；新增封存、延迟加分与提示音基准回归检查。

- 根据真机试玩优化「掐秒挑战」：交接页显示目标，按一次 OK 直接计时；整场胜利展示 3.5 秒或按 OK 后返回首页，仅从首页开启下一场。增加非计时阶段的原创背景音乐和六种按键／游戏反馈音，默认开启、首页可切换；计时打断音乐及此前音效，变暗或退出时停止声音。补充纯音频测试及 LVGL 平局渲染覆盖。后续试玩将默认音量由 40 调至 75/100，回合庆祝和详细成绩均停留等待 OK，不自动推进。

- 新增首版「掐秒挑战」固件页面：经主机测试的纯游戏模型、带时间戳的队列输入、同机轮流与 AI 练习、中文军事像素界面、回合／整场两层庆祝、worker 可选音效、电量标识及闲置降亮度；加入五项开发任务和无头 LVGL 渲染／退出重入检查。保留硬件 demo，菜单默认选中游戏。构建与 USB 写入校验不等于完整真机验收。

- 将浏览器原型重设计为「掐秒挑战」，围绕两名特工进行时间感训练比拼，采用军事街机风格，加入原创精细像素特工与前哨场景、交接时隐藏成绩、回合举拳庆祝与独立的整场结算；所有目标时长统一使用时间感训练语境及「开始计时／停止计时」提示，不附加虚构任务、行动剧情或课程副标题，先赢三轮者赢得本场对抗；计时画面保持静止，未修改固件。

- 新增独立的 Time Duel 同机双人交互原型，支持轮流挑战、成绩封存、AI 陪玩、可重复情景演示，以及回合和整场两层庆祝动画。

- 加入厂家为优特利 520mAh 电芯生成的 80 字节 CW2017 profile，并实现内容与更新标志检查、写入后校验、规定的重启时序以及有上限的 SOC 就绪等待。

- 按功能域整理文档并采用双入口：根目录 `AGENTS.md` 变为薄路由（只保留硬约束与任务路由），详细的 AI 开发工作流下沉到 `docs/development/ai-guide.md`，`agent-guide.md` 并入其中。为 `docs/development/` 增加二级分区（`engineering/`、`ci/`、`release/`），把 `plays/` 应用档案与 `experiences/` 移入带专属 README 的 `docs/reference/` 参考区；删除 `docs/software-design/`（空脚手架）；把 `assets/{fonts,images,music}/README` 三个叶子 README 并入 `assets/` README；把 `project-completion` 的六个子文档压平为单文件；并把每个目录统一为单一 README，消除所有 `INDEX` 文件与一处重复经验索引。所有交叉引用与文献链接已更新；未丢弃任何内容。

- 将小程序 BLE 安装兼容提升为二创模板强制契约：固定保护 `cardid`/Recovery 分区，
  保留上键持续 5 秒进入 Recovery 的 bootloader hook，并在 CI 强制校验合并镜像结构、
  分区表 MD5/范围、3 MB 应用上限和保护分区数据不入包。
- 规定多应用发布的 Release 标题约定：tag 按 `v<版本>-<应用名>`（如 `v0.1.0-voice-keychain`）命名，让 Release 标题同时带版本与应用名；发布成功后核对标题，保证一眼扫 Release 列表就能区分是哪个应用。
- 新增发布后收尾流程：`issue-suggestions` skill 用于把用户反馈作为 issue 提交到上游项目；`experience-pr` skill 用于把可复用的开发经验作为文档 PR 提交；新增 `docs/experiences/` 目录保存单条经验文件；并配套 `project-completion`、`file-issues` 与经验索引文档。
- 精简仓库根目录：将 GitHub 可识别的社区治理文档迁入 `.github/`，将变更记录迁入 `docs/`，同步全部引用，并在仓库检查中加入根目录文档白名单。
- 全仓库文档语言规范：所有维护中的 Markdown 默认 `.md` 文件使用英文，简体中文使用配对的 `.zh_CN.md`，双方提供语言切换；静态检查会阻止缺失配对、缺失切换链接或英文默认页混入中文正文。
- AI 开发流程一期：精简按任务加载的上下文入口，统一本地/CI 验证脚本，新增 PR 自动构建与模板，并提交依赖锁文件以提高构建可复现性。
- PR 审查修复：GitHub Actions 固定到完整 commit SHA，构建与发布 job 按最小权限拆分，同步 checkout 关闭凭证持久化；补充 Feature Request / Usage Question issue 表单；启用并修正私密安全报告兜底说明；清理 README 路径、CI 触发条件与历史分支描述漂移。
- 语言规范变更：commit 标题、PR 标题与 body 由"默认中文"改为**使用英文**（`docs/contribution/commit-and-pr.md` 更新）；中文写作规范（全角标点）适用范围剔除 PR/MR 描述（`doc-conventions.md` 更新）。
- CI 构建改造：`build-firmware.yml` 显式传入 `SDKCONFIG_DEFAULTS=sdkconfig.defaults` 再 `idf.py build`，由 defaults 启用自定义分区表（`CONFIG_PARTITION_TABLE_CUSTOM=y`，文件名为 `partitions.csv`）；`CONFIG_ESPTOOLPY_HEADER_FLASHSIZE_UPDATE` 改为 `n`，再用 `idf.py merge-bin -o build/FoloToy-AI-Passport-full.bin` 合并可直刷完整固件；产物精简为仅 full.bin；`actions/cache` 升级到 v5 以消除 GitHub Actions Node.js 20 弃用警告；CI 文档同步更新。
- 合并上游 PR #6（wireless-low-power-demos）以解决 PR #4 冲突：引入无线/低功耗 demo（`main/demo_wifi.c`、`demo_ble.c`、`demo_radio.c`、`demo_low_power.c`）、`partitions.csv`（NVS/PHY/3 MB factory-app 分区）、`main/CMakeLists.txt`/`main.c`/`demo.h`/`sdkconfig.defaults` 更新；同步硬件指南的 Wi-Fi/BLE/低功耗章节；README 能力契约表补充 Wi-Fi/Bluetooth LE/Low power 三项（中英双语）。
- 提交规范补充：`docs/contribution/commit-and-pr.md` 明确 PR 标题与 commit 标题使用相同的 Conventional Commit 格式和英文祈使句，不用名词短语当标题。
- CI 与文档清理：`sync-main.yml` 移除 `test_mode` 残留模板注释；`docs/development/coding-conventions.md` 将「Redis TTL」条目泛化为「缓存组件」条目（当前固件无 TTL 约束需求，消除从模板带入的无关约定）。
- 补充通用规范（借鉴 Shinku）：`docs/contribution/doc-conventions.md` 新增中文全角标点规范（正文 `，`；`（`）`，代码/命令/路径保留英文原样）、凭证不入仓规范（token/密钥/私钥绝不入仓，提交前 git diff 扫描敏感前缀）、文件删除安全规范（删除走系统回收站，不用 rm -rf/git clean -fd）。
- 代码注释规范强化：`docs/development/coding-conventions.md` 补充完善注释要求——函数说明（用途/参数/返回值/副作用/线程上下文/内存所有权/初始化顺序）、变量说明（语义/取值范围/生命周期/同步要求）、逻辑注释（状态机/时序/寄存器/魔数依据），覆盖范围宁多勿少，中文注释保留英文技术术语。
- 文档去 AI 化：`docs/README.md` / `docs/README.zh_CN.md` 移除 AI 专属章节（Entry point、Source-of-truth、提需求格式、BSP 边界、Runtime invariants、验收交付格式、构建命令），README 只保留给人看的项目介绍、硬件能力契约、demo 案例与项目结构；构建命令章节删除（与 `docs/development/build-and-test.md` 重复）。
- 新增 `docs/development/agent-guide.md`：集中承载"AI 如何在本仓库工作"（上下文建立顺序、事实来源优先级、提需求格式、BSP 边界、运行时规则、交付格式），并链接 build-and-test 与硬件指南，不重复构建命令与验收矩阵。
- 同步更新索引：`AGENTS.md` 规则索引新增 agent-guide 条目；`docs/INDEX.md` 与 `docs/development/README.md` 新增 agent-guide 索引行。
- 文档补充：`docs/fork-guide.md` 说明「为什么根目录不放置 README」——根目录 README 预留给 fork 开发者自行放置（上游留空），fork 后可将自己的内容写入根目录 `README.md` 介绍 fork 后的项目；GitHub 显示优先级（根 README > docs/README.md）契合该预留意图。
- 分支合并：创建 `main-update` 分支（基于与上游一致的 main），将 `feature/repo-structure`、`ci/build-firmware`、`ci/sync-main` 三个分支合并进来，统一 docs 结构（CI 文档归入 `docs/development/`，workflow 文件随 ci 分支引入 `.github/workflows/`）；解决 development/software-design README 的 add/add 冲突。
- 合并后审查修复：`docs/INDEX.md` 补充 CI 文档索引；`docs/fork-guide.md` 修正 workflow 引用为 `.github/workflows/sync-main.yml`；`docs/README` 双语项目结构块补充 `.github/workflows/` 与 CI 文档说明。
- ci 分支 CI 文档路径调整：`ci/build-firmware` 的 `docs/software-design/CI-build-and-release.md` 与 `ci/sync-main` 的 `docs/software-design/CI-sync-main.md` 均移入各分支的 `docs/development/`（CI 属工程规范）；`docs/software-design/README.md` 保留为软件设计索引；feature 分支的 software-design 索引同步更新引用。
- fork 补充文档目录迁移：`assets/docs/` 移至 `docs/assets/`（文档素材归入 docs/ 更合理），新增 `docs/assets/.gitkeep` 空目录占位；同步更新 AGENTS.md / INDEX / doc-conventions / fork-guide 的路径引用。
- 文档结构调整：根目录不再放 README——上游英文 README 移入 `docs/README.md`、中文移入 `docs/README.zh_CN.md`（GitHub 从 docs/ 识别主 README）；原 `docs/README.md` 根总索引更名为 `docs/INDEX.md`；同步更新 AGENTS.md / CONTRIBUTING / SUPPORT / fork-guide / doc-conventions 的路径引用。
- 初始化项目文档：新增 `AGENTS.md`、`CLAUDE.md` 和 `CHANGELOG.md`。
- 仓库结构规整：上游英文 `README.md` 更名为 `README.en_US.md`，保留 `README.zh_CN.md`。
- 新增目录骨架：`docs/`（software-design / hardware-design）、`assets/`（fonts / images / music，各含 `README.md`）、`skills/`。
- 将上游硬件开发指南归位到 `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`。
- 文档规范：子目录 readme 统一为大写 `README.md`；补充 fork 用户约定（main 只动根 README）。
- 扩展 fork 用户约定：`main` 分支允许修改根目录 `README.md` 和 `assets/docs/`（README 不足以说明项目时存放补充文档与素材）。
- 新增 `assets/docs/` 目录约定：上游 main 只保留空目录 `.gitkeep`，内容文件仅存在于 fork；使用方法规范写入 AGENTS.md「给 fork 用户」约定。
- CI 文档迁移：`docs/software-design/CI.md` 从本分支移除，迁至 `ci/build-firmware` 分支并改名为 `docs/software-design/CI-build-and-release.md`。
- 补充 `main` 分支策略说明：解释 `main` 保持干净的两大原因（与上游同步无冲突 + 多小项目按分支整理）；例外——执意 main 开发需停用 CI 自动同步；提醒 fork 用户默认 action 关闭需手动启用（此条为整个 CI 的通用要求，统一写入 AGENTS.md）。
- 文档拆分：将 `AGENTS.md` 按主题拆为公共文档——新增 `docs/contribution/`（doc-conventions.md、commit-and-pr.md）与 `docs/development/`（build-and-test.md、coding-conventions.md），新增 `docs/fork-guide.md`；`AGENTS.md` 精简为简介 + 项目概述 + 必读文档索引。
- 同步更新索引：`docs/software-design/README.md`、`README.en_US.md` / `README.zh_CN.md` 的 `docs/` 目录说明。
- 参考 cindy 仓库文档组织完善索引：新增 `docs/README.md` 根总索引；AGENTS.md 规则索引按触发场景改写（附触发条件）；`docs/contribution/` 与 `docs/development/` 的 README 补充收录标准。
- 引入社区治理文档（参照 cindy 改写，放仓库根目录）：新增 `CONTRIBUTING.md` / `.zh_CN.md`（贡献指南，针对 ESP-IDF/AI agent/fork 场景改写）、`CODE_OF_CONDUCT.md` / `.zh_CN.md`（贡献者公约）、`SECURITY.md` / `.zh_CN.md`（安全报告流程）、`SUPPORT.md` / `.zh_CN.md`（支持渠道）；AGENTS.md 与 docs/README.md 同步引用。
