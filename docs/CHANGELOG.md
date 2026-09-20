<p align="right">
  <a href="CHANGELOG.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Changelog

## Unreleased

- Fixed manual takeover during assisted corridor turns leaving a permanent off-grid heading: left/right now target the 45-degree heading grid while preserving camera easing and the current position, so later corner assistance and wall observation remain available.

- Fixed resuming after cancelling an incomplete corner incorrectly bypassing its assistance. Only a completed corner is bypassed; returning to the approach heading allows the same corner to guide the player again without resetting position.

- Added a playable Exit 8 finale before the completion panel: the eighth correct decision opens a terminal corridor with stairs and daylight, retaining the same three-key walk/pause/observe controls. Only reaching the exit completes the run; score 8 remains locked during the approach, and replay requires a separate OK after completion. The shared C implementation also drives the web preview; physical-device acceptance is pending.

- Fixed the Exit Corridor completion screen failing to refresh after the eighth correct decision: a phase change now schedules the final HUD redraw even when gameplay stops or the frame is throttled. Previously the screen could remain at Exit 7 while the next OK silently started a new run. Scoring and wrong-choice resets are unchanged.

- Added the native Exit Corridor 3D observation game as a firmware page and default boot menu option: features three-key controls (turn left, turn right, walk/stop), eight consecutive correct passages to escape, eight original anomalies, entry-relative turn-back decisions, and Chinese HUD, title, and entrance guide text. Existing demos, duel mode, and permanent Recovery remain intact.

- Implemented an optimized 240×320 software raycaster and renderer for ESP32-C3: uses fixed-point raycasting, sign projection, and surface calculations to reduce floating-point overhead on the no-FPU target. Employs continuous light-gray painted walls with subtle fixed perpendicular contrast for clear corner crease definition without subpixel grid shimmer, plus on-demand single-frame C decoding from Flash-embedded Rocketbox commuter sprites.

- Added smooth navigation and observation assistance: auto-walk rounds corners along a 0.85 m arc before stopping toward the next leg, with OK pause/resume and manual key overrides. Turning square to a wall smoothly centers nearby nominal fixtures (including fixtures visible at 45 degrees up to 1.2 m) at speed-capped rates; fixed anchor positions are identical across normal and anomalous scenes so assistance never hints at answers.

- Upgraded display pipeline and on-demand screenshot streaming: replaced the persistent 40,800 B capture buffer with on-demand serial row streaming (`FAP_SCREENSHOT_V1`), reclaiming internal RAM to support two 40-row DMA display buffers (38,400 B total) that overlap rendering with SPI transmission. Streamed I8-to-RGB565 LVGL decoding in 5-row chunks, bit-packed alpha masks, and out-of-memory handling reduce allocation pressure; audio coexistence and prolonged play remain device checks.

- Added shared WebAssembly preview and comprehensive test suites: web demo compiles the exact C game logic and software renderer with native/Wasm parity checks. Host tests validate observation geometry, rounded corner arcs, entry-relative scoring (including 7-to-8 completion versus 7-to-0 reset distinctions), 64 camera portal transitions, and renderer memory bounds.

- Expanded Time Challenge targets from 1.0–3.0 seconds to 1.0–6.0 seconds while retaining 0.5-second steps and the target-plus-three-second automatic stop. Firmware, browser free play, legacy prototype data, product requirements, sharing copy, and target-range regression coverage now use the same 11 values.

- Aligned the browser review page with the completed same-device two-player experience: removed the AI-practice selector and copy, fixed the simulator to pass-and-play, marked UP unused, replaced the stale unavailable-battery marker with a clearly disclosed preview value, and repaired automatic timeout/celebration/final transitions after the mode cleanup. Sound, one-press handover, sealed settlement, round celebration, persistent results, and match return-home behavior now mirror the tested two-player flow.

- Removed the rectangular matte around Time Challenge agents in both firmware and browser preview. The reproducible asset pipeline now converts the edge-connected source matte into transparency, stores agent poses as RGB565A8 while retaining the RGB565 background, and regression-checks descriptor formats, byte sizes, and Alpha planes.

- Prepared Time Challenge for community publishing and direct sharing: added the read-only `FAP_SCREENSHOT_V1` serial framebuffer service, a host-tested fragmented command matcher, a generated exact-3:4 cover, and a bilingual release/install guide. The service keeps a 120 x 160 evidence frame from real LVGL flushes, avoiding a full-screen DRAM allocation on the no-PSRAM target; it never flashes, resets, or changes game state.

- Applied the reviewed handover layout to firmware: removed both agent portraits and VS from handover, and reused the target screen's 192 x 105 duration card with 40 px numerals. Retained one-press timing, hidden first-player results, timeout copy, and independent battery refresh. Added pixel-parity regression checks across every configured target, both players, and normal/timeout handovers; aligned browser status and requirements with firmware.

- Fixed Time Challenge's missing battery percentage on first entry: the LVGL tick now observes the worker's battery cache independently of game transitions. Only the top-right label changes when its normalized value changes; delayed readings, unavailable/recovered values, and reentry no longer require a button or whole-screen redraw. Added host regression coverage and real-LVGL checks for unchanged game pixels, animation progress, and teardown safety. Battery I/O remains in the worker; physical cold-start acceptance is still required.

- Refined the browser handover screen for review: removed the two-agent VS artwork and reused the target screen's full-size duration card, while keeping one-press start and sealed first-player results. The subsequent firmware implementation is recorded above.

- Synchronized the interactive browser preview with current firmware: device-derived pixel assets and screen layouts, one-press handover, sealed confirmation before scoring, a separate 1.5-second celebration, persistent numeric results, and match victory returning home. Added frozen nine-screen navigation, five review scenarios, matching procedural music/cues, and portable browser tests. Browser fonts, audio, timing, battery, and the menu boundary remain explicitly non-device evidence.

- Clarified Time Challenge settlement: both attempts now remain sealed without changing the score until a separate OK. Confirmation plays the round-win animation, which then opens detailed results automatically; the next round still requires OK. Raised only background-music amplitude by 40% (about 2.9 dB), keeping the 75/100 output volume and all six cue waveforms unchanged. Added sealed-result, delayed score, and cue-baseline regression checks.

- Refined Time Challenge after device playtesting: handover shows the target and starts timing with one OK; match victory returns home after 3.5 seconds or on OK, and the next match starts only from home. Added original background music outside timing and six button/game cues, on by default with a home sound toggle. Timing interrupts music/previous cues; dimming and exit stop sound. Added portable audio tests and LVGL tie coverage. Follow-up playtesting raised default volume from 40 to 75/100 and made both round celebration and detailed results wait for OK; neither automatically advances.

- Added the first Time Challenge firmware page: a host-tested portable game model, timestamped queued input, pass-and-play and AI practice, Chinese military-pixel screens, separate round/match celebrations, optional worker-driven sound, battery status, and idle dimming. Added a five-task delivery plan and headless LVGL rendering/teardown checks. Existing hardware demos remain accessible; the game is preselected in the menu. Build and USB write validation are distinct from full device acceptance.

- Redesigned the browser prototype as Time Challenge, a military arcade time-sense training match between two agents, with original detailed pixel agents and outpost artwork, hidden estimates during handover, raised-fist round wins, and separate match celebrations. Every target uses the same training premise and Start Timing / Stop Timing hints, without fictional missions, an operational story, or a course subtitle. The first to three round wins takes the match. Timing remains visually still; firmware is unchanged.

- Added a standalone Time Duel interaction prototype for two players sharing one device, with turn-taking, sealed results, AI practice, replayable scenarios, and separate round and match celebrations.

- Added the supplied 80-byte CW2017 profile for the specified 520 mAh cell, including content/update-flag checks, verified writes, the required restart sequence, and bounded SOC-readiness polling.

- Reorganized the documentation by function area with a dual entry point: the root `AGENTS.md` is now a thin router (hard constraints + task routing only) and the detailed AI workflow lives in `docs/development/ai-guide.md`; `agent-guide.md` was folded in. `docs/development/` gained a second level (`engineering/`, `ci/`, `release/`), and the `plays/` application archive and `experiences/` moved into a `docs/reference/` area with a dedicated README. Removed `docs/software-design/` (empty scaffold); folded the three `assets/{fonts,images,music}/README` leaves into the `assets/` README; flattened the six `project-completion` sub-documents into a single file; and unified each directory to a single README, eliminating every `INDEX` file and a duplicated experience index. All cross-references and bibliographic links were updated; no content was dropped.

- Made mini-program BLE install compatibility a template-level invariant: fixed
  protected `cardid`/Recovery partitions, retained the five-second UP-key
  Recovery boot hook, and added CI validation for merged-image structure,
  partition MD5/ranges, the 3 MB app limit, and protected payload exclusion.
- Documented a release-title convention for multi-app releases: name tags as `v<version>-<app-name>` (e.g. `v0.1.0-voice-keychain`) so the release title carries the version and the app, and confirm the title after the release is published so a release list is scannable by app.
- Added a post-release follow-up workflow: an `issue-suggestions` skill for filing user feedback as issues against the upstream project, an `experience-pr` skill for submitting reusable development experience as a documentation PR, a `docs/experiences/` directory for per-entry experience files, and supporting `project-completion`, `file-issues`, and experience-index documents.
- Simplified the tracked repository root: moved GitHub-recognized community documents into `.github/`, moved the changelog into `docs/`, updated every reference, and added a root-document allowlist to repository checks.
- Repository-wide language policy: every maintained Markdown default `.md` file is English, Simplified Chinese uses a paired `.zh_CN.md`, and both provide language switches. Static checks reject missing peers, missing switches, and Chinese prose in English defaults.
- Phase one of the AI development workflow: streamlined task-based context routing, unified local/CI validation, added PR checks and a template, and committed the dependency lock for reproducible builds.
- PR review fixes: pinned GitHub Actions to full commit SHAs, split build/release jobs by least privilege, disabled persisted sync checkout credentials, added Feature Request and Usage Question forms, clarified private security-report fallback, and corrected stale README, CI-trigger, and branch descriptions.
- Changed commit titles, PR titles, and PR bodies from Chinese-default to English; updated the Chinese punctuation rule so it no longer applies to PR descriptions.
- Reworked `build-firmware.yml` to pass `SDKCONFIG_DEFAULTS=sdkconfig.defaults`, enable `partitions.csv`, preserve the 8 MB image header, merge a flashable `FoloToy-AI-Passport-full.bin`, publish only that artifact, and use Actions cache v5.
- Integrated upstream PR #6 to resolve PR #4 conflicts: Wi-Fi, Bluetooth LE, radio lifecycle, and low-power demos; a 3 MB factory partition; build/menu/configuration updates; hardware-guide coverage; and bilingual capability tables.
- Defined English imperative Conventional Commit formatting for both commits and PR titles.
- Removed stale sync-workflow template comments and generalized an irrelevant Redis TTL rule to cache components.
- Added Chinese punctuation, credential safety, and recoverable file-deletion conventions.
- Expanded source-comment requirements for functions, state, ownership, concurrency, timing, registers, and magic values.
- Removed AI execution instructions from product READMEs so they remain human-facing product and repository overviews.
- Added `docs/development/agent-guide.md` as the focused AI workflow guide.
- Updated `AGENTS.md`, `docs/INDEX.md`, and the development index for the agent guide.
- Documented why the root README path is reserved for fork owners and how GitHub README precedence supports it.
- Created `main-update` from the upstream-aligned baseline and combined the repository-structure, firmware-CI, and upstream-sync work.
- Corrected the merged documentation index, workflow path, project tree, and CI references.
- Moved CI documentation from software design to `docs/development/`.
- Moved fork-only documentation assets from `assets/docs/` to `docs/assets/`.
- Moved the upstream English/Chinese project READMEs under `docs/` and renamed the documentation catalog to `docs/INDEX.md`.
- Initialized `AGENTS.md`, `CLAUDE.md`, and `CHANGELOG.md`.
- Standardized the initial project README language filenames.
- Added the `docs/`, `assets/`, and `skills/` directory structure.
- Moved the upstream hardware guide into `docs/hardware-design/`.
- Standardized subdirectory README capitalization and introduced fork conventions.
- Allowed fork-owned root README and supplemental documentation content on fork `main`.
- Added and documented the fork-only supplemental-document directory.
- Moved the build CI document to its dedicated CI branch before consolidation.
- Documented clean-`main` reasons, the direct-development exception, and Actions enablement for forks.
- Split the original agent rules into contribution, development, and fork documents with a compact root index.
- Updated software-design and project README references for the new documentation structure.
- Added the documentation catalog and task-triggered routing based on the earlier repository model.
- Added bilingual contribution, code-of-conduct, security, and support documents tailored to this ESP-IDF and fork workflow.
