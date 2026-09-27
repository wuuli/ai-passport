[简体中文](game-integration.zh_CN.md) · **English**

# Game integration with the upstream baseline

This derivative integrates upstream `1051209` with Exit Corridor and Time Challenge. The games retain their gameplay, assets and portable C cores. Their navigation is separate from the upstream hardware-test application.

## Entry points and controls

The default build uses `main/game_main.c` and starts at the Exit Corridor title. During Corridor play, hold OK to return to its title; hold OK on that title to open the game selector. UP and DOWN select Exit Corridor or Time Challenge, and a short OK opens the selected game. Holding OK in Time Challenge returns to the selector.

The selector and both games use their own LVGL screens. `CONFIG_PASSPORT_HARDWARE_DEMO=y` selects the upstream `main/main.c` diagnostics instead; hardware test screens are reference code, not part of game navigation.

Button callbacks enqueue timestamped events. A single LVGL timer consumes them and ticks the active game; page changes advance a generation so queued events cannot operate the next page. Input overflow stops the active game. No game UI is called from the BSP button task. The application-lifetime battery/audio worker shares only atomic data and does not retain page pointers.

## Display and installation compatibility

The integration adopts upstream display initialization rollback, rounded-corner masking, 80 MHz LCD SPI, and one 40-row LVGL draw buffer. This frees 19,200 bytes compared with the former double buffer; actual frame cadence and visual effects still require device measurement. Both games inset their status and battery labels to keep text inside the 30-pixel corner mask; browser shells show the same rounded safe area.

The fork deliberately retains its existing 3 MB factory partition, `cardid` and Recovery regions and UP-key boot hook. These are this application's compatibility choices, not upstream template requirements. The hook does not install a Recovery image. The configured-layout gate validates the actual partition table; no original-firmware backup is a prerequisite for flashing. Do not infer that an old device is compatible without checking its layout, or erase the chip as a routine installation step.

## Demo and device acceptance

Follow the [shared-C/Wasm acceptance SOP](../development/engineering/game-demo-to-device-acceptance.md). Exit Corridor shares both model and renderer; Time Challenge shares its C model and uses a browser visual reference for its LVGL page. Browser parity does not establish device font rendering, memory margin, physical-key response, audio or display cadence.

The changed entry/exit path, game re-entry, battery display, upstream display changes and both complete game flows require fresh physical-device acceptance. Previous device reports describe their original candidates and do not validate this integration.

## Integration candidate checks (2026-09-27)

Static/host checks and the ESP-IDF 5.5.3 firmware/layout gate passed. The application is 1,502,336 bytes; the merged image is 1,567,872 bytes. The merged-image SHA-256 is `a388aff41e3f5da2663f44f877aa5ac2fd14f2b76403d46b66664fa616536fb4`; its matching ELF/MAP and manifest are retained under `build/firmware/<sha256>/`. This identifies a local pre-commit build, not an installed or released version.

Dispatcher tests cover navigation, stale input, queue overflow and a press arriving after queue drain: the tick uses the batch-start timestamp so it cannot overtake that queued press. Native C/Wasm trace tests pass for both games. Physical-device tests for this integration are **NOT RUN**.
