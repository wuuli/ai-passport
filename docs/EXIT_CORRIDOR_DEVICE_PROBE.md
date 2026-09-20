English | [简体中文](EXIT_CORRIDOR_DEVICE_PROBE.zh_CN.md)

# Exit Corridor Device Validation Checklist

The game ships at native 240x320. The earlier separate probe page and scaled-renderer proposal are superseded by `main/demo_corridor.c`. See [recorded results](DEVICE_VALIDATION_EXIT_CORRIDOR.md); targets below are not claims of achieved performance.

## Measurement

- Warm up for five seconds, then record at least 30 seconds each of walking, turning, corner assistance and observation centering. Disable screenshot capture during timing.
- Existing `corridor` logs report submitted frames per window, average renderer CPU time, last-frame floor/wall/sprite CPU time, refresh time, free/minimum heap and largest contiguous block. Submitted frames are not panel-completion FPS. Zero serial bytes are not a stability result.
- Record firmware and asset hashes, display configuration, scene and sampling duration. Do not compare different scenes as a performance improvement.
- Still unmeasured: panel-completion interval p50/p95/p99, late-frame rate, and concurrent I2S/audio DMA pressure with two 40-row display buffers. Sustained 20 FPS is a target, not a release guarantee.

## Physical Checks

- Play from 0 to 8; verify a wrong decision resets to 0, and the correct eighth passage opens the completion screen.
- Check both corners, both entry directions, all eight anomalies, gentle fixture centering, immediate key override and pause/resume.
- Inspect plain-wall motion and perpendicular-face contrast on the physical panel, not only serial screenshots.
- Repeat entry, long-OK exit and re-entry. Check Time Challenge sound/timing and the existing display/audio demos for regressions.
- Preserve UP/DOWN turn and short-OK walk/stop semantics. No new BSP release event or second control scheme is required.

## Install Safety

Verify the release image and device partition table before app-only USB installation. Keep a rollback copy of the verified application partition. Never read or overwrite identity/NVS data unnecessarily.

The app starts at `0x10000` and must fit 3 MB. Keep `cardid` at `0x356000`, permanent Recovery at `0x700000` and the five-second UP boot hook unchanged. A successful build or USB transfer does not establish normal gameplay.
