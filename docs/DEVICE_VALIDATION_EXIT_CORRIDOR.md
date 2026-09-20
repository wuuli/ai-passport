English | [简体中文](DEVICE_VALIDATION_EXIT_CORRIDOR.zh_CN.md)

# Exit Corridor device validation

Status: Firmware installed on 2026-09-20. The current installation runs the verified 2,312,240-byte plain-wall build at `0x10000`. Startup is verified, while long-session physical play and the latest corner face contrast refinement remain unverified on hardware.

## Current installation status

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
