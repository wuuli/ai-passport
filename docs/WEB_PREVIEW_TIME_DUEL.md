English | [简体中文](WEB_PREVIEW_TIME_DUEL.zh_CN.md)

# Time Challenge firmware web preview

The interactive [web preview](http://127.0.0.1:8098/time-duel-v2.html) runs the firmware game state machine as WebAssembly. Start the local server below before opening it; direct `file://` access cannot load the WebAssembly module and assets. The integrated multi-game launcher is also available at [games.html](http://127.0.0.1:8098/games.html).

## Shared code and architecture

- `main/duel_clock.c` and `main/duel_clock.h` compile unchanged to `prototype/time-duel/firmware/duel.wasm`. The pure C state machine drives round phases (`HOME`, `TARGET`, `TIMING`, `HANDOVER`, `SEALED`, `ROUND_END`, `RESULT`, `MATCH_WIN`), target intervals (1.0 to 6.0 seconds in 0.5-second steps), duration estimation, judging (`abs(estimate - target)`), match scoring (first to three round wins), alternating start players, and seeded AI Gaussian opponent behavior (`sigma = 0.12 * target`).
- The thin C bridge in `tools/duel_web_bridge.c` exports input handlers (`web_key`, `web_handle`), tick execution (`web_tick`), lifecycle controls (`web_init`, `web_home`, `web_exit`), state inspection (`web_state`), and deterministic review fixtures (`web_set_fixture`).
- `prototype/time-duel/firmware/runtime.mjs` handles WebAssembly loading, exports binding, and state decoding for browser integration.
- The web shell in `prototype/time-duel-v2.html` provides a 240x320 browser visual reference for the LVGL page layout in `main/duel_ui.c`, using the established Metal Slug inspired pixel-art direction (red-headband and blue-beret agents, sunset outpost, worn olive hardware frame, and sand-gold typography); native C and WebAssembly tests establish model parity rather than visual rendering equality.
- Audio synthesis in `prototype/time-duel-audio.js` replicates the chiptune background music and six sound cues via the Web Audio API, following the silence rules during duration estimation.
- `prototype/time-duel/firmware/manifest.json` records SHA-256 digests for required native clock sources and the compiled `duel.wasm` artifact. Startup checks verify module integrity against this manifest.

The browser layer acts as a visual and audio presentation shell around the shared C clock engine without implementing an independent duplicate game loop.

## Run and rebuild

Local preview checks require Python 3.10+ and Node.js 18+. Viewing the built preview requires only a modern browser and local HTTP server; no embedded toolchain is needed.

```bash
python3 tools/serve_corridor_web.py --port 8098
# Open http://127.0.0.1:8098/time-duel-v2.html or http://127.0.0.1:8098/games.html
python3 tools/build_duel_web.py --check
node tests/test_duel_web.mjs
node tests/test_duel_preview.mjs
```

The server binds strictly to loopback and serves the preview and asset directories.

After modifying native clock sources or the web bridge, rebuild with the official [WASI SDK](https://github.com/WebAssembly/wasi-sdk/releases) (validated with 34.0):

```bash
python3 tools/build_duel_web.py --sdk /path/to/wasi-sdk
./tools/validate.sh --static
# Activate ESP-IDF 5.5.3 before the complete gate:
./tools/validate.sh
```

Keep `duel.wasm`, `runtime.mjs`, and `manifest.json` together in `prototype/time-duel/firmware/`. Four WASI runtime license notices accompany the module in `prototype/time-duel/firmware/` (`COMPILER_RT_LICENSE.txt`, `MUSL_COPYRIGHT.txt`, `WASI_LIBC_LICENSE.txt`, and `WASI_MIT_LICENSE.txt`). Asset documentation and provenance are maintained in the [asset guide](../assets/README.md#time-duel-artwork).

## Controls and navigation

In the Time Challenge web preview:
- **Short OK** (Space / Enter / button click, triggered on press): performs in-game phase actions on key press (ensuring timing starts and stops immediately on press rather than release), including starting the match from the home screen, starting and stopping duration timing, confirming sealed results, skipping celebration animations, and advancing to the next round.
- **Hold OK for 1 second**: exits directly to the game selector boundary.
- **DOWN key**: toggles chiptune music and sound cues on the home screen.
- **UP key**: disabled during play; toggles duo / AI mode on the home screen.

### Firmware boot and navigation flow

The unified firmware entry point `main/game_main.c` integrates all games:
1. The firmware boots directly into Exit 8 (Corridor) by default.
2. Holding OK for one second during Exit 8 gameplay returns to the Corridor title screen.
3. Holding OK on the Corridor title screen opens the Game Launcher selector (`main/game_launcher.c`).
4. In the Game Launcher, press UP / DOWN to navigate between Exit 8 and Time Challenge, then press OK to enter the selected game.
5. In Time Challenge, holding OK for one second exits directly back to the Game Launcher selector.
6. The browser launcher at `prototype/games.html` mirrors this selector UI and navigation flow.

### Pass-and-play round flow

- **Handover screen**: reuses the target duration card at 40 px font size, keeps player 1's estimate hidden, and allows player 2 to start timing immediately with a single OK press.
- **Sealed results**: estimates and winner status remain sealed until an explicit OK confirms settlement.
- **Celebration and scoring**: 1.5-second round celebration with impact banner, persistent numerical results table until OK, and 3.5-second match victory before returning home.

## Validation and boundaries

The web preview is intended for visual, auditory, and interaction flow evaluation; browser execution does not constitute physical hardware device acceptance. The test suite verifies native C and WebAssembly state parity, not hardware gameplay feel or physical timing precision.

Deterministic trace parity between native C and WebAssembly is verified by `node tests/test_duel_web.mjs`, passing across 39 snapshots and 95 trace commands with zero state error. A browser smoke check on 2026-09-27 covered selector navigation, duo timeout and sealed-result confirmation, detailed results, return home, and AI mode/startup without console errors. Full blind-player acceptance and device validation remain pending; these checks do not establish LVGL rendering equivalence.

Key differences from physical hardware:
- **Battery**: the web preview displays a mocked `--%` indicator; the physical device reads live battery voltage via ADC.
- **Timing and button input**: the web prototype uses browser high-resolution timestamps (`performance.now()`). On physical hardware, the BSP samples an ADC resistor ladder polled by `esp_timer` task callbacks, capturing microsecond timestamps with `esp_timer_get_time()`.
- **Audio**: the browser synthesizes waveforms through the Web Audio API; the physical device streams 16 kHz mono PCM over I2S to the ES8311 codec.
- **Hardware and partition constraints**: the browser does not simulate ESP32-C3 hardware constraints (no PSRAM), the fork's configured 3 MB application partition boundary, SPI/DMA display bus timing, resistor ladder button debounce, or deep sleep power management.
- **Firmware safety**: the web build does not alter or flash device partitions.

The browser input adapter is checked with the real Wasm core and a controlled clock by `node tests/test_duel_preview.mjs`: press/release and repeat handling, sealed results, mode changes, input interruption, and long-press exit. These checks do not measure physical key latency.
