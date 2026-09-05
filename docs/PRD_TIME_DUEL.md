<p align="right">
  <a href="PRD_TIME_DUEL.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Time Duel — Product Requirements Document

## Current scope

Two players share **one AI Passport** and estimate the same target duration. The estimate with the smaller absolute error wins the round. The first player to win three rounds wins the match.

The current review artifact is [the standalone interaction prototype](../prototype/time-duel-v2.html). It runs in a browser and does not implement firmware or device communication. The earlier two-device BLE concept and its implementation tickets are superseded.

## Confirmed requirements

- One device hosts the two-player match. No NFC or BLE pairing is required.
- Targets are drawn from `{1.0, 1.5, 2.0, 2.5, 3.0}` seconds, in 0.5-second steps.
- Each player controls the start and stop of their estimate through a device button.
- Timing screens do not show elapsed time.
- A match is first to three round wins; tied rounds do not count toward the five scored rounds.
- Round wins and match victories have separate celebrations.

## Prototype interaction assumptions

The current prototype uses **pass-and-play with the shared OK button**. This input choice and the details below are recommendations for review, not additional user-confirmed requirements.

The board's UP, DOWN, and OK buttons share one ADC resistor ladder. The BSP defines mutually exclusive voltage windows, so two simultaneous presses cannot be assumed to produce independent events. Pass-and-play fits the existing input interface.

### Round flow

1. Announce the target and the starting player. That player presses OK to start.
2. Show the active player's name and a neutral mascot. Pressing OK again locks the estimate.
3. Keep the estimate hidden and ask the player to pass the Passport. The next player presses OK when ready.
4. Show the same target again. The next player presses OK to start and again to stop.
5. Compare both locked estimates using `abs(estimate - target)`.
6. Play a roughly 1.5-second round celebration naming the winner, then show both estimates, errors, and the updated score. OK can skip the celebration.
7. Press OK to continue. Alternate the starting player on each attempt, including tied replays. A tie retains the target, score, and round number.
8. Once either player reaches three wins, show the final-round celebration and result first, followed by the match victory screen. OK starts a new match; long-press OK returns to the entry screen.

### Timing and fairness

- Hide both estimates until both attempts have ended, including during the handover.
- No countdown, elapsed counter, progress indicator, or periodic animation that acts as a metronome during an attempt.
- Automatically stop an unfinished attempt at `target + 3 seconds`; use that duration as the estimate and identify the automatic stop at settlement.
- Ignore repeat presses within a 200 ms debounce window.
- Firmware should capture `esp_timer_get_time()` at `BSP_BTN_PRESS`, then notify the game logic. Do not perform audio or display work in the callback.
- The browser prototype uses `performance.now()` and whole-millisecond estimates. Browser timing is not proof of device accuracy.
- Passing the device and alternating the starting player reduce order effects but cannot prevent a player from observing the other person's button presses.

## Visuals and sound

Reuse the pixel sky, grass, ink-outlined panels, and TV mascot from `main/ui_pixel.c`. Preserve a non-overlapping top-right battery indicator. The browser's case shape and battery reading are illustrative.

- **Round win:** name the winner on the shared screen, bounce the mascot, show a ROUND WIN banner, and pulse the score. Use a neutral DRAW state for ties.
- **Match victory:** name the champion, show a trophy and confetti, preserve the final score, and offer rematch or exit.
- **Memory:** ESP32-C3 has no PSRAM. Implement animations using `lv_anim` transforms of objects; do not use full-screen RGB565 frame sequences.
- **Sound:** four short cues for start, stop, win, and loss, played from a worker task. No background music. The prototype provides optional synthesized preview sounds.

## Single-player practice

The same device supports an AI opponent. The player takes their attempt, then the AI produces an independent estimate before settlement. The proposed model is Gaussian error with `sigma = k * target`; the prototype uses `k = 0.12`. Smaller `k` means a more accurate, stronger opponent. Difficulty tiers remain subject to playtesting.

## Firmware implementation direction

- `main/duel_clock.c/h`: hardware-independent timing state machine, handover, judging, scoring, and AI model.
- `main/duel_ui.c`: shared-screen LVGL presentation and both celebration layers.
- `main/demo_duel.c`: page lifecycle, button events, audio dispatch, and demo registration.
- `tests/test_duel_clock.c`: host coverage for state transitions, ties, timeouts, alternating turns, early match completion, and seeded AI behavior.
- No game-specific BLE protocol, pairing task, or central-role configuration change is needed.

Preserve the 3 MB app limit, protected cardid/Recovery partitions, existing long-press return convention, and the five-second UP-key recovery hook. Stop page-owned tasks, callbacks, and timers before deleting the UI.

## Acceptance targets

These are implementation requirements, not completed validation results.

- Host tests pass for the pure game model and seeded AI behavior.
- The complete firmware gate passes, preserving partition and recovery compatibility.
- One physical device completes a two-player match, including hidden handover, ties, missing-stop timeout, both celebration layers, rematch, and long-press exit.
- Button timing, display legibility, sound, and memory usage are checked on the actual device.

## Open review points

1. Confirm the proposed pass-and-play input and hidden-result handover.
2. Tune target range and AI difficulty after physical playtests.
3. Tune animation poses, cue duration, and visual density on the 240 × 320 display.
