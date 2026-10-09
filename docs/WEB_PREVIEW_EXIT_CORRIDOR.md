English | [简体中文](WEB_PREVIEW_EXIT_CORRIDOR.zh_CN.md)

# Exit Corridor firmware web preview

The main [preview](http://127.0.0.1:8098/) executes the current firmware game and renderer as WebAssembly. Start the local server below before opening it; direct `file://` access cannot load the WebAssembly module and assets. This replaces the independent JavaScript prototype, including visits with `?renderer=webgl`. The earlier JavaScript prototype runtime has been retired; character baking tools and package validation tests are retained.

## Sound

The firmware and browser share `main/corridor_sound.c`: quiet ventilation, footsteps driven by actual player displacement (including automatic corners), and a short completion tail only after reaching the final doorway. Stopping or looking around does not trigger steps; anomaly type and right/wrong choices never select a sound. The title is silent. Sounds are synthesized as 16 kHz mono PCM without sampled audio assets.

The native adapter sends snapshots to the existing background I/O worker; button and LVGL callbacks never write PCM. The browser starts audio after a user gesture, displays an explicit activation state, and has a Sound toggle plus a volume slider. Desktop playback applies a separate gain (4 at the default 40%, maximum 10; 40% preserves the previous full-scale listening level); the shared source PCM is unchanged. The firmware applies a saturating 6× gain once in the Corridor output worker, matching the browser slider at 60% digitally, while retaining codec volume 75%. This does not establish equal perceived loudness between the computer and device speakers. Time Duel output is unchanged. Input gestures also restore the preview focus state so an embedded-browser focus event cannot leave audio paused. Losing focus, hiding the tab, pausing review or leaving the page cancels scheduled audio; resuming does not replay old steps. Browser PCM consistency does not establish device loudness, codec latency, frame rate or RAM margin: these require physical-device acceptance.

Host checks cover movement/corner triggers, observation without footsteps, resets, muted history, completion and browser teardown. Native and Wasm produced 112,000 identical PCM samples. The synth and tracker occupy 80 bytes together, excluding the worker, mailbox and PCM buffer. Desktop loudness was accepted at the slider's 60%; hardware timing and acoustic quality are separate checks.

Installed bundle (2026-09-29): `build/firmware/0af8d8336e49fffb5c1d2cc8a830cd079a04f41a2c34d9af9f269160005248b1/`; merged SHA-256 `0af8d8336e49fffb5c1d2cc8a830cd079a04f41a2c34d9af9f269160005248b1`; matching ELF SHA-256 `13b4fc1e67ae73708fc7ea89e2c52874902da646d5ef0330c431eb4f494fcf4d`. App 1,507,440 bytes; merged image 1,572,976 bytes. Build PASS; Host tests PASS; Device tests PASS for the bounded USB scenarios below. The complete gate used tracked defaults and ESP-IDF 5.5.3. All three segmented-write hashes passed and startup ELF prefix `13b4fc1e6` matched. NVS, PHY, card identity and recovery ranges were not written. No commit, push or publication was performed. On 2026-09-30 the creator confirmed the sound was normal during overall device playtesting. Unverified: instrumented acoustic latency, physical switch debounce boundaries and long-duration load testing.

### Device audio scheduling acceptance (2026-09-29)

The rejected candidate produced seven idle-task watchdog reports during active gameplay. Matching symbols placed samples in LVGL's flush busy-wait; free internal heap was 126,624 bytes. The evidence points to CPU scheduling starvation rather than allocation failure. The BSP now blocks on a DMA completion semaphore, and the audio worker runs at priority 5 above LVGL's 4 while blocking on I2S writes. The measured 420-byte stack margin with diagnostics justified increasing this worker from 3,072 to 3,584 bytes; PCM and framebuffer sizes are unchanged.

On the exact installed image, a 130-second USB test acknowledged all 15 commands, covering entering, walking, stopping, left/right observation, both automatic corner stops, a correct passage to score 1, and three returns to title/re-entries. Twenty audio windows showed a maximum feed gap of 1,649 microseconds, synthesis time of 431 microseconds, and write wait of 14,935 microseconds. Steady supply was 15,993–16,033 samples/s; initial refill windows can run above the nominal 16,000. The minimum worker stack margin was 932 bytes. No watchdog, panic or audio-write failure occurred.

Stopped intervals kept step counters unchanged (11, 22, 29, and 40); later windows returned to quiet ambience rather than repeating footsteps. Each return to title logged audio idle and stopped frame generation. Steady gameplay measured 9.68–11.00 fps, 125,992 free internal heap bytes, a 115,792-byte minimum and a 106,496-byte largest block. These are bounded scenario measurements, not a performance guarantee. After timing tests, a separate USB screenshot confirmed the corridor/HUD rendered; the device was returned to its silent title and the serial port released. Acoustic output was not recorded, so these checks do not establish that all audible crackles have disappeared. On 2026-09-30 the creator completed device playtesting and confirmed the sound was normal; this is user listening acceptance, not an acoustic instrument measurement.

### Browser-first audio acceptance

Desktop playback was accepted before installation, but the earlier `cf5f4aec...` device candidate was subsequently rejected for delayed playback and crackling. Its 12-second title-only startup check did not exercise gameplay and did not establish audio acceptance. The installed scheduling fix is identified above.

Open **Sound audition** in the existing game page to play six repeatable clips: the complete enter/walk/stop/title sequence, stationary ambience, walking then stopping, an automatic corner, stationary observation, and the final exit. Each clip drives an isolated instance of the actual C game and synthesizer, then encodes its PCM as 16 kHz mono WAV at the 60% reference gain. It pauses walking without changing the player's position or score. The native audio controls play the generated clip independently of live scheduling; a download link allows repeated listening. The reference clips have a fixed 60% gain, independent of the live-game slider. Only one playback path runs at a time.

Host checks verify unclipped reference PCM, WAV round-trip samples, no footsteps during observation, the automatic stop and completion silence. Thirty repeated enter/walk/stop/title sequences verify prompt onset, return to ambience after stopping and sustained silence on the title. These checks do not establish acceptable timbre. The live scheduler now preserves contiguous timestamps when fewer than 5 ms remain, uses 100 ms lookahead, and exposes underrun count and maximum gap in `getAudioState()`. This fixes a demonstrated browser scheduling gap, not a proven cause of the device sound complaint.

The revised C/Wasm sound removes the constant 100 Hz lamp buzz and lowers ventilation noise. At the 60% reference, stationary RMS drops from -32.7 to -45.7 dBFS; the six fixtures have no clipping. The user clarified that continuous sound occurred after stopping movement, consistent with the former always-on ambience, but that alone did not explain the device crackling. Device scheduling evidence is recorded separately above.

## Shared code and assets

- `main/corridor_game.c` and `main/corridor_render.c` compile unchanged. Three-key movement, rounded corners, diagonal-visible observation centering, entry-relative decisions, wrong-choice resets, and eight-success completion use the same C implementation.
- The browser loads the same 312,689-byte `commuter-device.bin`. Palette, sprite lookup and notice data compile from the firmware includes. Walls use plain light-gray paint with depth lighting and skirting, without a tiled lookup or PNG atlas. Transverse connector walls (`side >= 2`) dim by one shade for corner face contrast. Native C and Wasm parity is verified; 64 same-camera portal seams and dual-corner contrast tests pass. The current firmware, including this contrast refinement, was installed on the physical device on 2026-09-27; its direct-to-title boot screen was captured over serial.
- Generated `presentation.json` extracts the 16px body and four-glyph 30px title fonts and validates title/HUD copy against `demo_corridor.c`. The Canvas shell expands indexed pixels with RGB565 quantization and draws the dark opening, daylight-colored completion, and HUD at native 240×320. The opening describes the physical keys by position (top, middle, bottom OK) and explains automatic corner stops; both endpoint layouts keep the action at y=252 and return hint at y=287. The action fades in once over 800ms and remains steady. Stair and exit rendering are unchanged by the UI.
- `manifest.json` records source and artifact SHA-256 hashes. Startup validates the Wasm, presentation and sprite bytes. The static gate rejects a stale build after native source changes.

The thin C bridge exposes input, ticks, output and explicit review fixtures. Browser glue handles pointer/keyboard input, visibility, loading, display and development controls. It does not implement another game state machine.

## Run and rebuild

Python 3.10+, Node.js 18+ and a C11 compiler are needed for local preview checks. Opening the already generated preview needs only a modern browser and HTTP server; no SDK is required.

```bash
python3 tools/serve_corridor_web.py --port 8098
# Open http://127.0.0.1:8098/
python3 tools/build_corridor_web.py --check
node tests/test_corridor_web.mjs
```

The server binds only to loopback and serves the preview and its asset subtree. Do not start a second server on an occupied port.

After changing a manifest-listed native source or asset, rebuild with the official [WASI SDK](https://github.com/WebAssembly/wasi-sdk/releases) (validated with 34.0):

```bash
python3 tools/build_corridor_web.py --sdk /path/to/wasi-sdk
./tools/validate.sh --static
./tools/validate-games.sh
# Activate ESP-IDF 5.5.3 before the complete gate:
./tools/validate.sh
```

Keep `corridor.wasm`, `presentation.json` and `manifest.json` together. Runtime license notices accompany the module in `prototype/exit-corridor/firmware/`; character provenance remains in the [asset guide](../assets/images/exit-corridor/README.md).

## Controls and review

UP / up arrow turns left 45 degrees on press; DOWN / down arrow turns right 45 degrees on press; short OK / Space / Enter enters from the title screen or toggles walking on release; holding OK for one second returns to the web title. Only one key acts at a time. Focus loss, hidden tabs and cancelled touches stop walking.

Development review is closed by default, reveals answers and pauses play when opened. It provides all nine normal/anomaly fixtures, paired forward/return views of the overhead sign, the reported 45-degree poster case, corner approaches and boundary/final-round cases. Resume simulation to inspect, or press OK to start walking. Returning to blind play starts fresh at zero; injected fixtures cannot be carried into a blind run. Magnification changes CSS size only.

During an assisted corner, OK pauses/resumes the arc. Left/right cancels it and stops translation; the target heading uses the nearest 45-degree grid direction plus the requested turn, with the camera easing toward it. Position and displayed camera heading do not snap on that keypress. Normal manual turns remain relative 45-degree steps.

The passage decision updates the game score as soon as the boundary is crossed. The upper-left number keeps the previous value until the camera has turned toward the next corridor near its centerline, so the HUD does not reveal the new exit before the in-world sign comes into view. Manual turning can reveal it too; once revealed, the number remains stable if the player looks back.

## Exit sequence

The eighth correct decision enters `EC_EXITING` (3), not `EC_CLEARED` (2). The new corridor keeps the crossing position and corner assistance, displays Exit 8, and leads to a short staircase and daylight. The same direction keys turn 45 degrees and stop; OK toggles walking. Score 8 is locked, no more anomalies are selected, and turning back cannot trigger another judgment. Walk up the stairs and across the two-meter landing to the doorway to show the persistent completion panel; a separate OK then starts a stopped score-0 run. This is a hardware-sized adaptation of walking out of the passage, not a frame-for-frame reproduction of the original game's ending.

For a quick controlled review, choose the normal scene with the final forward fixture, or red lights with the final return fixture. Press OK to cross, wait for the assisted corner to stop facing the stairs, and press OK again to walk out. Pause and look around anywhere along the approach. The exit geometry is rendered by the same bounded C renderer, with no extra framebuffer or staircase texture allocation.

## Validation and boundaries

Corner re-acceptance on 2026-09-20 checked all four approach directions through visible browser controls: the stops were (0.85, -26.8, 90 degrees), (-0.85, 2.8, -90 degrees), (0, 1.95, 0 degrees), and (0, -25.95, 180 degrees). Each faced the next corridor leg on its centerline. The audit also reproduced an off-grid heading after mid-arc manual takeover; this is corrected in shared C and covered by native and Wasm regression tests. Physical-key timing and on-device motion still require hardware acceptance.

On 2026-09-20 the updated native-versus-Wasm trace used 5,729 commands and 163 sampled frames, including 24 paused mid-corner takeover and same-position recovery cases. Of 12,518,400 indexed pixels, two differed with maximum color-channel delta 2/255; maximum state difference was 1.20e-7. This covers round choices, both entry directions, the playable exit and doorway completion, turns, pauses and observation; it is sampled equivalence, not proof for every possible run.

Browser UI checks confirmed the reported diagonal poster case ends at X=0, Z=-20, yaw=90 degrees; normal 7-to-8 completion and replay; anomalous forward 7-to-0 reset; and anomalous return 0-to-1.

The new finale was exercised using visible review controls and real button clicks: normal forward and red-light return choices both entered Exit 8, rounded the corner, and walked to their respective doorways before completion. Mid-stair 45-degree observation, explicit replay to stopped score 0, and incorrect forward 7-to-0 were checked separately. Screenshots at the default desktop viewport and 390x844 showed the scene and three keys without overlap. The preview is left at the stair approach for user acceptance. These are controlled fixtures, not a fresh blind eight-round run. Static/host and ESP-IDF firmware gates passed; renderer ASan/UBSan checks passed. No device was flashed, and staircase frame rate, perceived motion and RAM margin remain physical-device checks.

Before the exit sequence was added, controlled browser acceptance on 2026-09-20 verified the old immediate completion panel, explicit replay, and a red-light forward 7-to-0 failure. That historical check does not cover the new staircase. The device-only missing redraw is covered separately by `tests/test_corridor_demo.c`, which executes the actual firmware tick with UI stubs, now including active exit rendering and a throttled doorway completion; Canvas success alone cannot verify LVGL refresh on hardware.

The browser does not emulate ESP32 RAM pressure, SPI/DMA timing, physical ADC debounce, LVGL composition, battery readings or the device menu. Its battery is `--` and long OK returns to the preview title. The web loop submits at most 20 frames/s, and its visible performance counter is explicitly not a device measurement. Native framebuffer comparison excludes Canvas HUD composition. No device firmware is modified or flashed by the web build.

## Scope of local performance simulation

The current Wasm preview shares the game, renderer and sound source while device and browser adapters run separately. It does not run the device's FreeRTOS, LVGL, SPI/DMA or I2S paths, so it did not cover the task starvation caused by display flush busy-waiting. Low browser frame rates or CPU throttling alone do not establish that device tasks can refill audio on time.

Additional local stress checks can enforce a game allocation budget and inject allocation failure; model scheduling and buffers using measured render/transfer durations, then inject delay and CPU occupation to detect audio underruns; and replay walking, stopping, corners and teardown. Such a model must distinguish priorities, blocking waits and interrupt completion events, and be calibrated against device measurements. This preview does not currently implement that full stress model. Existing host flush-wait boundary tests and device feed-gap checks are not a complete emulator.

Physical bus timing, actual heap fragmentation, speaker quality and prolonged load still need device checks. Use three stages: shared C/Wasm gameplay acceptance, calibrated resource/scheduling stress tests, and physical-device acceptance. Simulation helps expose failures earlier; board measurements determine final acceptance.

## USB device acceptance

The game firmware accepts newline-terminated `FAP_KEY_V1 LEFT`, `RIGHT`, `OK`, and `BACK` commands over its physical USB serial connection. Send the full `FAP_KEY_V1` prefix on each line. These represent the top, middle, short bottom OK, and long OK actions. They enqueue the same button events as the physical keys; only the LVGL dispatcher touches game UI. Invalid commands are ignored and queue overflow stops movement. This developer interface does not emulate switch debounce or human timing.

Run bounded enter/walk/stop/look/corner/title/re-entry sequences while collecting `game_audio` feed rate, maximum gap, synthesis/write duration, stack margin, and PCM peak alongside the game frame/heap logs. Capture screenshots separately, since screen transfer disturbs timing. USB tests cannot confirm the speaker's acoustic output.

## Fork game validation

`tools/validate.sh` covers generic repository/BSP checks and firmware packaging;
it does not invoke game tests. Run `./tools/validate-games.sh` separately for both
games' native logic, rendering, sound, lifecycle, launcher and C/Wasm parity checks.
Game-specific resource scenarios, when present, belong in that application gate;
the reusable resource model remains independent of either game.

The game gate needs Node.js 18+, Python 3.10+ and a C11 compiler. Checking existing
Wasm artifacts does not need WASI SDK. `.github/workflows/game-checks.yml` runs the
same command for pull requests and pushes to `main` that change application code,
shared BSP code, assets, previews, tests, tools, build configuration or that workflow.
It also supports manual dispatch. Documentation-only changes skip it, so do not
require this path-filtered status for unrelated changes. A game release requires
both the generic gate and the game gate; neither replaces physical-device testing.
