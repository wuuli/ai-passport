English | [简体中文](TASKS_EXIT_CORRIDOR.zh_CN.md)

# Exit Corridor development tickets and acceptance

**Current entry (EC-32):** The web preview ([`prototype/exit-corridor.html`](../prototype/exit-corridor.html)) executes firmware C code directly as WebAssembly; see [sync and rebuild instructions](WEB_PREVIEW_EXIT_CORRIDOR.md). Earlier JavaScript prototype runtimes have been retired; character baking tools and package validation tests (`packed-sprites.js` / `.test.cjs`) are retained.

Development baseline: `feature/exit-corridor`, local `main` at `2e6813b`. Core research and design rationale are documented in [reference research](RESEARCH_EXIT_CORRIDOR.md).

## Delivered capabilities

| Capability / Ticket | Implementation summary | Verification outcome |
| --- | --- | --- |
| **Reference & Homage** (EC-01) | Established independent homage boundaries to *The Exit 8*, citing official primary sources. | Documented and approved. |
| **Game Rules & Continuity** (EC-02, EC-24) | Eight consecutive correct crossings escape; error resets to zero; entry-relative directional scoring; seamless doglegs. | Host test suites PASS: boundary decisions, entry-relative scoring, continuous doglegs, 3,600 NPC motion frames, and 36 final-round cases. |
| **Original Anomalies** (EC-03) | Eight distinct reversible anomalies (missing door, poster eyes, inverted poster, extra vent, red lights, tall commuter, staring commuter, absent commuter); stable baseline. | All 8 anomalies inspected at native 240 x 320. Forced-sample scenarios 1,2,2,0,8 PASS. |
| **Controls & Navigation** (EC-04, EC-14) | Single 3-key contract: UP/DOWN turn 45 degrees on press, short OK walks/stops on release, 1-second OK hold exits. | Input state machine, focus blur, and visibility handling PASS. |
| **Continuous NPC Motion** (EC-08, EC-11, EC-18) | MIT Rocketbox offline bake: 8 directions x 16 walk frames + 1 resting pose (136 frames, 312,689 B package). On-demand single-frame C decoding into 4,608 B + 576 B alpha mask. | Deterministic encoding and roundtrip decode PASS in `packed-sprites.test.cjs`. |
| **2.5D Software Raycaster** (EC-16, EC-22) | Integer raycasting, plain light-gray walls, depth lighting, skirting, and sprite projection without tiled lookup or PNG atlas overhead. | 8241 fixture-free plain wall pixels stable across 9 positions PASS. |
| **Display & Memory Architecture** (EC-15, EC-29) | Native 240 x 320 portrait output. Two 40-row DMA buffers (38,400 B total) funded by replacing 40,800 B capture buffer with on-demand row streaming (`FAP_SCREENSHOT_V1`). | Free internal heap was recorded at 27,572 B (minimum 23,060 B) in historical live play samples. |
| **Rounded Corner Assistance** (EC-30) | Auto-walk smoothly rounds all four corners along a 0.85 m radius arc before stopping toward the next leg. OK pauses/resumes; direction keys override. | Host traversal, approach offsets, pause, and manual override PASS. |
| **Observation Centering** (EC-31) | Turning square to a side wall smoothly aligns nearby fixtures (near <=0.65 m, diagonal-visible <=1.2 m) at <=1.2 m/s. Identical anchor positions across normal and anomaly scenes. | 162 near + 36 diagonal-visible cases and ASan/UBSan PASS. |
| **Corner Face Contrast** (EC-32) | Transverse connector walls (`side >= 2`) dim by one shade, preserving visual depth and corner orientation without texture lookups. | 64 same-camera portal seams and dual-corner contrast tests PASS. |
| **WebAssembly Parity** (EC-32) | Firmware C game and renderer compiled unchanged via WASI SDK into `corridor.wasm`. Native 240 x 320 presentation shell. | 1,343 trace commands across 59 frames match native with 0 changed pixels and 2.98e-8 state delta. |
| **Firmware Installation** (EC-28) | Verified 2,312,240-byte plain-wall application flashed to physical ESP32-C3 at `0x10000`. | Normal boot, menu selection, and serial screenshot exchange PASS. |

## Module contract

Active entry: [`prototype/exit-corridor.html`](../prototype/exit-corridor.html).

- `prototype/exit-corridor/firmware/app.mjs`: browser input handling, animation loop, review UI binding.
- `prototype/exit-corridor/firmware/display.mjs`: native 240 x 320 RGB565 scanout quantization and UI text rendering using firmware bitmap font.
- `prototype/exit-corridor/firmware/input.mjs`: three-key input state machine (ADC-style key handling: directions on press, short OK on release, 1-second hold exit).
- `prototype/exit-corridor/firmware/runtime.mjs`: Wasm lifecycle, state reflection, integrity checks against `manifest.json`.
- `prototype/exit-corridor/firmware/bridge.c`: thin C wrapper exporting game and renderer functions to WebAssembly.
- `prototype/exit-corridor/firmware/corridor.wasm`: compiled C game engine and software renderer.
- `prototype/exit-corridor/firmware/presentation.json`: font bitmaps and UI copy extracted from firmware.
- `prototype/exit-corridor/firmware/manifest.json`: source/artifact SHA-256 integrity manifest.
- `prototype/exit-corridor/firmware/style.css`: responsive handheld enclosure styling.
- `main/corridor_game.{c,h}`: authoritative game logic, state machine, rounded corners, observation centering.
- `main/corridor_render.{c,h}`: 2.5D software raycaster, plain walls, corner face contrast, sprite projection.
- `main/demo_corridor.c`: ESP32-C3 hardware integration and LVGL UI.
- `prototype/exit-corridor/tools/pack-sprites.cjs`: offline sprite encoder generating `commuter-device.bin`.
- `prototype/exit-corridor/packed-sprites.js` / `packed-sprites.test.cjs`: retained package format verification tests.
- `prototype/exit-corridor/tools/bake.html`, `bake-server.py`, `test_bake.py`, `prototype/exit-corridor/vendor/`: offline character baking pipeline.

## Playtest and reproduction

Serve the corridor preview and its public assets on loopback, and navigate to `http://127.0.0.1:8098/`:

```bash
python3 tools/serve_corridor_web.py --port 8098
python3 tools/build_corridor_web.py --check
node tests/test_corridor_web.mjs
```

Run repository checks and host test suites:

```bash
PYTHONDONTWRITEBYTECODE=1 ./tools/validate.sh --static
```

Title states only the goal. The four general rules appear on the entrance wall ($Z = -3.5$); blind play reveals no answers. Development review provides forced anomaly fixtures and state inspection for testing.

## Remaining acceptance checklist

| Verification target | Current status | Boundary |
| --- | --- | --- |
| Automated host & Wasm gate | PASS | Complete static checks and native/Wasm parity verified. |
| On-device plain-wall installation | PASS | 2026-09-20 application installed and verified at `0x10000`. |
| Corner face contrast refinement | PASS in host/Wasm; **PENDING on device** | Verified in host tests and Wasm preview; not yet flashed to hardware. |
| User blind-play fairness | PENDING | Native 240 x 320 blind playtest feedback remains open. |
| Physical key & grip comfort | PENDING | Continuous turning and long-session grip on the resistor ladder remain unverified. |
| Sustained >=12 FPS on silicon | PENDING | Historical production sample measured 10.7–12.7 submitted FPS; full sustained benchmark open. |
| Full 0-to-8 playthrough | PENDING | Complete human physical playthrough from start to finish remains unverified. |
