English | [简体中文](RESEARCH_EXIT_CORRIDOR.zh_CN.md)

# Exit Corridor: reference and prototype decisions

Research date: 2026-09-18. Visual direction corrected on 2026-09-19: Wolfenstein was a rendering-technique reference, not an art direction. The browser target is a believable underground passage close in atmosphere to The Exit 8, with refined materials, lighting, anatomy, and continuous animation; gameplay still tests observation, doubt, and physical turn-back with three buttons. This is an original prototype and firmware implementation inspired by *The Exit 8*, not a reproduction of its proprietary assets or code.

## Verified reference

| Finding | Primary source | Implication |
| --- | --- | --- |
| The player is trapped in an endless underground passage and must observe surroundings to reach Exit 8. | [Official Steam listing](https://store.steampowered.com/app/2653790/The_Exit_8/) | A stable, ordinary place must be learned before changes carry meaning. This implication is our design interpretation. |
| Do not overlook anomalies; turn back immediately if one is found; do not turn back when none is found. | [Developer-supplied Steam description](https://store.steampowered.com/api/appdetails?appids=2653790&l=english) | Movement expresses the decision; an explicit true/false quiz would lose the core interaction. |
| It is a short walking simulator inspired by Japanese underground passageways, liminal spaces, and back rooms. | Official Steam listing | Use everyday station objects, repetition, and restraint; do not make combat or pursuit the main loop. |
| The advertised play time is 15–60 minutes. | Official Steam listing | This is the original's published range, not a target measured for our prototype. |
| Options include mouse sensitivity and camera shake. The description recommends zero camera shake for discomfort and disabling motion blur at 30 FPS. | Official Steam listing | Default to no head bob or motion blur; offer deliberate, controllable looking. |
| KOTAKE CREATE's own site links *The Exit 8* to Steam app 2653790. | [Developer site](https://www.kotakecreate.com/) | Confirms authorship and the correct reference; app 2903560 is a different game. |

These pages were read directly. The [official PlayStation listing](https://www.playstation.com/en-us/games/the-exit-8/) independently repeats the same core instructions and walking-simulator positioning. The publisher URL initially attempted returned 404; it is not evidence. No claim below relies on an exhaustive anomaly guide or an unviewed video.

## What is interpretation, not verified original implementation

The public descriptions read here do not establish the exact anomaly count, spawn weights, repeat protection, first-round behavior, random seed, boundary geometry, or full failure-state implementation. The eight-success/reset-to-zero model comes from our agreed adaptation and remains explicitly a prototype rule. Do not label a fixed probability, guaranteed learning round, or individual original-looking anomaly as an authenticated recreation.

The useful design hypothesis is **recognition -> comparison -> doubt -> physical commitment -> another apparently familiar passage**. A normal round needs to remain convincing; danger effects should not reveal the answer on every abnormal round. The game allows a player to stop and examine, with no score timer.

## Current architecture and design facts

- **Core implementation**: The shipping route runs identical C game logic (`main/corridor_game.c`) and 2.5D software raycasting (`main/corridor_render.c`) on both the ESP32-C3 firmware (`main/demo_corridor.c`) and the browser via WebAssembly (`prototype/exit-corridor/firmware/corridor.wasm`).
- **Screen & environment**: Designed for the native 240 x 320 portrait LCD. The corridor uses plain light-gray painted walls with distance lighting and dark skirting. Transverse connector walls (`side >= 2`) are dimmed by one shade for clear corner face contrast without requiring procedural tile lookups or texture atlases.
- **Character rendering**: Pre-rendered 2D sprite package (`assets/images/exit-corridor/commuter-device.bin`, 312,689 B) derived offline from a pinned MIT-licensed Rocketbox civilian model (commit `0943055db6ec570bcef9f2c8b41c9e5467c808f9`, 8 directions x 16 walk frames + 1 resting pose, 48x96 cells). Decoded on demand in C into a single active frame buffer (4,608 B frame + 576 B alpha mask). See [asset README](../assets/images/exit-corridor/README.md).
- **Controls & navigation**: Single physical three-key contract (UP turns left 45 degrees on press, DOWN turns right 45 degrees on press, short OK walks/stops on release; 1-second OK hold exits to title/menu). Auto-walk rounds all four corners along a 0.85 m radius arc before stopping toward the next leg, with OK pause/resume and key overrides. Turning square to a wall smoothly centers nearby nominal fixtures (up to 1.2 m for diagonal-visible fixtures) at speed-capped rates (<=1.2 m/s). Anchor positions are identical across normal and anomalous scenes.
- **Rules & anomalies**: Eight original anomalies (missing door, poster eyes, inverted poster, extra vent, red lights, tall commuter, staring commuter, absent commuter). Boundary crossing evaluates entry-relative forward versus turn-back choices. Eight consecutive correct choices clear; an incorrect choice resets score to zero. Merely turning does not submit a choice. General rules appear on a physical notice at the entrance wall (Z = -3.5); blind play reveals no anomaly lists or answers.
- **WebAssembly preview**: Entry at [`prototype/exit-corridor.html`](../prototype/exit-corridor.html), served on loopback via `python3 tools/serve_corridor_web.py --port 8098`. Parity checks verify bit-for-bit equivalence between native C and Wasm. See [web sync guide](WEB_PREVIEW_EXIT_CORRIDOR.md).

## Hardware constraints and resource accounting

| Target constraint | Shipping implementation fact |
| --- | --- |
| ESP32-C3, no PSRAM; 8 MB Flash, 3 MB app partition | Entire game engine, font subset, and character assets live within the application Flash image (~2.31 MB). Two 40-row display DMA buffers (38,400 B total) reside in internal RAM. Flash assets: commuter package 305.36 KiB, RGB565 sprite LUT 64 KiB, Noto Sans SC font subset. Plain walls require 0 B Flash LUT. |
| ST7789P3, 240 x 320 portrait RGB565, 40 MHz SPI | Full native resolution rendering. The native indexed framebuffer is 77,824 B (including a 1,024 B ARGB8888 palette). |
| Two 240 x 40 RGB565 DMA buffers (38,400 B total) | Two 40-row double buffers overlap rendering with SPI transmission, funded by replacing the former 40,800 B capture copy with on-demand serial row streaming (`FAP_SCREENSHOT_V1`). |
| Working memory in internal heap | Renderer working state occupies 13,264 B split allocations on a 64-bit host (device structure size is target-specific and reported in runtime logs), including a 576 B 1-bit transparency mask. Scanout uses five RGB565 rows and palette (2,912 B static RAM). Free internal heap was recorded at 27,500–27,572 B (minimum 23,060 B, largest block 14,848 B) in historical runtime samples. |
| Three ADC keys on GPIO0 | Non-blocking ADC input handling. Direction keys turn immediately on press; short OK acts on release; 1-second OK hold exits. |

Measured performance, firmware installation, and remaining hardware verification items are recorded in the [device validation document](DEVICE_VALIDATION_EXIT_CORRIDOR.md).
