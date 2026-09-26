English | [简体中文](WEB_PREVIEW_EXIT_CORRIDOR.zh_CN.md)

# Exit Corridor firmware web preview

The main [preview](http://127.0.0.1:8098/) executes the current firmware game and renderer as WebAssembly. Start the local server below before opening it; direct `file://` access cannot load the WebAssembly module and assets. This replaces the independent JavaScript prototype, including visits with `?renderer=webgl`. The earlier JavaScript prototype runtime has been retired; character baking tools and package validation tests are retained.

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
