English | [简体中文](DEVICE_VALIDATION_EXIT_CORRIDOR.zh_CN.md)

# Exit Corridor device validation

Latest observation: the connected device was checked on 2026-10-08 without flashing. Records below identify each candidate; older installation and performance records remain historical.

## 2026-10-08 USB regression and partial calibration

The connected game's application descriptor and boot ELF prefix match the verified
archive `0af8d8336e49fffb5c1d2cc8a830cd079a04f41a2c34d9af9f269160005248b1`
(ELF `13b4fc1e67ae73708fc7ea89e2c52874902da646d5ef0330c431eb4f494fcf4d`,
ESP-IDF 5.5.3). This is an identity match, not a full Flash checksum. Read-only
inspection confirmed a 3 MB factory application at `0x10000` and a separate
Recovery application at `0x700000`; no Flash was written or device identity read.
The matching local archive passed layout and hash verification.

[Sanitized evidence](assets/resource-stress/device-observations-20261008.json)
contains three sessions of 164, 154 and 37 seconds, with all 33 USB key commands
acknowledged in order. They exercise entry, walking, observation, automatic corner
stop, title/launcher exit and re-entry. The final session explicitly records
manual walk/stop/resume/stop and stable position after stopping. Both long sessions
record a correct normal passage from 0 to 1. This does not cover a complete
0-to-8 run, all anomalies or physical button debounce.

No panic/watchdog, input-overflow or audio format/write-failure markers were
captured. Five title transitions logged audio idle. Screenshot capture occurred
outside the timing sessions; the final captured game view remained responsive.
The logs do not expose a DMA underrun counter and cannot establish acoustic quality.

| Observation | Result |
| --- | --- |
| Complete playing windows | 53; partial entry/title windows excluded |
| Submitted frame rate | 9.621-11.323 FPS; not panel-completion cadence |
| Renderer window-average range | 35,532-50,426 us; not per-frame P95/P99 or worst case |
| Audio windows | 57 after excluding initial buffered/partly pre-session windows; first usable window after activation/idle also excluded from steady-rate comparison |
| Steady audio supply | 15,993-16,019 samples/s |
| Maximum observed synthesis elapsed time | 504 us |
| Maximum between-write processing gap / write wait | 2,084 / 15,098 us; not measured audible latency |
| Playing free heap / lifetime minimum / largest block | 125,992 / 115,792 / 106,496 bytes |
| Minimum audio-task stack headroom | 932 bytes |

The second session observed 504 us synthesis versus 433 us in the first and
431 us in the old profile. The supplemental session also widened the maximum
renderer window average from 50,133 to 50,426 us; observed refresh averages widen
the wait estimate from 50,337 to 50,360 us. The partial profile now includes these
observations. Do not reduce them to the first run or treat them as worst-case bounds.
The fixed reference-workload model was rerun after the parameter update; measured
costs do not feed back into the C/Wasm game state.

USB lifecycle checks pass within this bounded scope. Resource calibration remains
`NOT RUN`/partial: per-frame tails, separate CPU/transfer waiting, effective PCM
capacity/prefill, panel completion, and independent prediction-error validation
still need instrumented measurements. Speaker quality, physical-button timing,
long-session fragmentation and complete playthrough are unverified. Existing logs
cannot fill those gaps merely by collecting more window averages.

## 2026-09-20 installation history

- **Installed firmware**: 2026-09-20 plain-wall application (2,312,240 bytes) flashed to ESP32-C3 at `0x10000`. The partition table matched the verified merged image; the previous 3 MB application was backed up locally. Bootloader, partition table, NVS, identity, and permanent Recovery were not modified.
- **Hardware verification**: Device rebooted normally. Screen capture confirms Corridor selected in the boot menu ([`firmware-plain-wall-menu-20260920.png`](../assets/images/exit-corridor/device-validation/firmware-plain-wall-menu-20260920.png)). [Installation metadata and write hashes](../assets/images/exit-corridor/device-validation/firmware-install-plain-wall-20260920.json).
- **Corner face contrast refinement**: Transverse connector walls (`side >= 2`) dim by one shade, preserving visual depth and corner orientation. Passed 64 same-camera portal seams and dual-corner contrast tests; native and Wasm are verified bit-for-bit consistent. **Status: verified in host tests and Wasm web preview, but not yet flashed to the physical device.**

## Historical performance measurements

Measurements on connected ESP32-C3 (revision 1.1, 8 MB Flash, no PSRAM) during corridor development:

| Firmware build / experiment | Submitted FPS | CPU rendering | LVGL refresh | Free heap (lifetime min) | Status / Result |
| --- | --- | --- | --- | --- | --- |
| **Initial playable build** | ~4.0 | 105–118 ms | not split | - | Baseline |
| **Memory fix (1-bit mask)** | 4.2–4.7 | 104–126 ms | - | 15,620 B (11,180 B) | Fixed contiguous RAM allocation failure |
| **Fixed rays/signs, single 20-row buffer** | 6.7–7.1 | 52–59 ms | ~85 ms | - | Intermediate rounded corners |
| **Wall LUT, two 10-row buffers** | 6.5–6.8 | scene dependent | ~96 ms | - | Rejected (cadence degradation) |
| **On-demand capture, two 40-row buffers** | 10.5–11.4 | 46–53 ms | ~38 ms | 27,500 B (23,060 B) | Retained (freed 40,800 B capture buffer) |
| **Fixed-point surface setup** | 11.0–12.1 | 42–49 ms | ~38 ms | 27,500 B (23,060 B) | Historical pre-plain-wall benchmark; retained setup |
| **Centered observation probe** | 12.3–13.0 | 36–39 ms | ~38 ms | - | Temporary key probe; removed from source |
| **Production live walk (55s sample)** | 10.7–12.7 | 37–51 ms | ~38 ms | 27,572 B (23,060 B) | Clean build without automatic probes |

Historical runtime samples: [production live sample](../assets/images/exit-corridor/device-validation/firmware-production-live-20260920.json), [fixed surfaces sample](../assets/images/exit-corridor/device-validation/firmware-fixed-surfaces-20260920.json). In historical runtime samples, free internal heap was recorded at 27,500–27,572 B (minimum 23,060 B) with largest block 14,848 B; no panic, watchdog, or allocation-failure markers were observed during measurements. Frame windows count submissions; they do not measure completed LCD DMA or frame-time percentiles.

## Historical 7-to-0 transition investigation

During earlier playtesting, a user reported a transition from score 7 to 0. Under the game rules, any incorrect boundary choice resets progress to 0; only a correct eighth judgment completes the game.

Because the running version at the time did not log individual anomaly and departure events, the historical incident is **unattributable** (cannot be confirmed as player error or a code defect). Subsequent diagnostic builds add serial logging for boundary decisions (`log_judgement`) without revealing answers on screen. A 36-case test suite validates all 9 scene states, both entry directions, and all departures at score 7; correct choices advance to 8 and incorrect choices reset to 0.

## Confirmed completion display defect (2026-09-20)

The user connected a device still displaying Exit 7 and walking. Passive diagnostics recorded `before=7 after=8 correct=1 phase=2` with `frames=0 refreshes=0`; the [screen](../assets/images/exit-corridor/device-validation/cleared-stale-hud-20260920.png) and [sanitized record](../assets/images/exit-corridor/device-validation/cleared-stale-hud-20260920.json) confirm successful completion with a stale HUD, not an incorrect choice. No reset or input was issued during capture. The firmware redraw condition stopped updating after `EC_CLEARED` without marking the screen dirty; the next OK would therefore restart behind an unchanged playing screen. The fix preserves a redraw request across phase changes and frame throttling. It has not yet been flashed or verified on the device. The replacement ending now includes a playable stairway before the persistent completion panel; see [web acceptance](WEB_PREVIEW_EXIT_CORRIDOR.md). Earlier unlogged incidents remain unattributed.

## Repeatable host screen capture

The host capture utility (`tools/capture_passport_screen.py`, tested by `tests/test_capture_passport_screen.py`) uses the `FAP_SCREENSHOT_V1` serial protocol. Capture streams 120 x 160 RGB565LE rows on demand under the LVGL lock, eliminating the former 40,800 B persistent capture buffer and freeing RAM for double-buffered LCD DMA.

```bash
PYTHONDONTWRITEBYTECODE=1 python3 tools/capture_passport_screen.py   --port /dev/cu.usbmodem2101 --timeout 10 --output /tmp/passport-capture-01
```

Each capture outputs a PNG, a raw-payload SHA-256 hash, and timestamped JSON metadata. Validation evidence: [initial menu capture](../assets/images/exit-corridor/device-validation/menu-20260919.png) ([metadata](../assets/images/exit-corridor/device-validation/menu-20260919.json)), [repeat tool capture](../assets/images/exit-corridor/device-validation/menu-tool-20260919.json), [plain-wall menu](../assets/images/exit-corridor/device-validation/firmware-plain-wall-menu-20260920.png).

## Remaining unverified boundaries

- **Physical movement comfort**: User comfort regarding auto-walk rounded corners (0.85 m radius) and observation centering (up to 1.2 m for diagonal fixtures) remains to be evaluated.
- **Sustained >=12 FPS**: Sustained >=12 FPS across long play sessions on ESP32-C3 silicon remains open.
- **Flashing corner face contrast refinement**: The latest corner face contrast update (`side >= 2` dimmed by one shade) is verified in host tests and Wasm, but has not yet been flashed to the device.
- **Full physical playthrough**: A complete physical 0-to-8 playthrough from start to finish on hardware remains unverified.
