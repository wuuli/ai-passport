<p align="right">
  <a href="PRD_TIME_DUEL.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Time Challenge — Product Requirements Document

## Current scope

Two players share **one AI Passport** and estimate the same target duration. The estimate with the smaller absolute error wins the round. The first player to win three rounds wins the match.

The [interactive browser preview](../prototype/time-duel-v2.html) now follows the implemented firmware screens and flow, including subsequent playtest refinements. [The task register](TASKS_TIME_DUEL.md) separates implementation from device evidence. No device communication is needed; the earlier two-device BLE concept is superseded.

The nine-step navigator and five scenarios pause on selected screens with explicitly labeled example data; OK resumes interaction, while the playback button starts real-time phase transitions. Free play uses the shared OK button or Space/Enter. The browser mirrors sealed confirmation, separate 1.5-second round celebration, persistent numeric results, and 3.5-second match victory returning home. It also models 60-second idle dimming and wake-only input. Browser fonts, audio output, the illustrative battery value, and the hardware-menu boundary are not device measurements. Run `node --test tests/test_duel_preview.cjs` (Node 22 or later) for portable browser-flow and cue-parity checks.

The handover screen removes the two-agent VS artwork and reuses the target screen's duration card at the same position and 40 px number size. First-player results stay sealed and one OK still starts timing. Firmware and browser now share this layout; the firmware's two screens call the same card builder.

## Confirmed requirements

- One device hosts the two-player match. No NFC or BLE pairing is required.
- Targets are drawn from 1.0 through 6.0 seconds, in 0.5-second steps.
- Each player controls the start and stop of their estimate through a device button.
- Timing screens do not show elapsed time.
- A match is first to three round wins; tied rounds do not count toward the five scored rounds.
- Round wins and match victories have separate celebrations.

## Interaction baseline

Use **pass-and-play with the shared OK button**. Physical playtesting confirmed the basic flow; the user requested direct start from handover, return home after a match, and music with button feedback.

The board's UP, DOWN, and OK buttons share one ADC resistor ladder. The BSP defines mutually exclusive voltage windows, so two simultaneous presses cannot be assumed to produce independent events. Pass-and-play fits the existing input interface.

### Round flow

1. Announce the target and the starting player. That player presses OK to start.
2. Show the active player's name and a neutral mascot. Pressing OK again locks the estimate.
3. Keep the estimate hidden, show the same target on the handover screen, and ask the player to pass the Passport.
4. The next player presses OK to start timing immediately, then again to stop. There is no separate readiness confirmation or second target screen.
5. Once both attempts finish, keep results sealed and wait for a separate OK. Until confirmation, do not disclose the winner, estimates, errors, or a changed score; play no win/loss cue.
6. On confirmation, compare `abs(estimate - target)`, update the score, and play the separate round-win animation (or neutral tie presentation). After 1.5 seconds, automatically show both estimates, errors, and score; OK may skip the animation. Do not merge the animation and detailed-result pages. The detailed result stays until OK, never automatically starting another round.
7. Press OK to continue. Alternate the starting player on each attempt, including tied replays. A tie retains the target, score, and round number.
8. Once either player reaches three wins, show the final-round celebration and result first, followed by the match victory screen. After 3.5 seconds, return to game home; OK can return sooner. Only another OK on home starts a new match. Preserve duo/AI mode. Holding OK for one second exits to the hardware menu, with an explicitly illustrative menu boundary in the browser.

### Timing and fairness

- Hide both estimates during handover and the sealed-result screen. Reveal the winner only after OK confirmation, then show the numeric results after the celebration.
- No countdown, elapsed counter, progress indicator, or periodic animation that acts as a metronome during an attempt.
- Automatically stop an unfinished attempt at `target + 3 seconds`; use that duration as the estimate and identify the automatic stop at settlement.
- Ignore repeat presses within a 200 ms debounce window.
- Firmware should capture `esp_timer_get_time()` at `BSP_BTN_PRESS`, then notify the game logic. Do not perform audio or display work in the callback.
- The browser prototype uses `performance.now()` and whole-millisecond estimates. Browser timing is not proof of device accuracy.
- Passing the device and alternating the starting player reduce order effects but cannot prevent a player from observing the other person's button presses.

## Training Premise

The game is named Time Challenge, without a course subtitle. Time-sense training describes the activity: two agents compete to estimate durations at a training camp.

The opening is: "Two agents. One time-sense training match. No clock: trust your sense of time and see who gets closer." Two agents share one terminal and take turns producing the same target duration. The smaller absolute error wins each round; the first to three wins is the match winner. Badges represent round wins, not ranks or a separate progression system.

The task is interval production: start and stop a specified duration without an external elapsed-time display. It practices duration estimation and action timing rather than reacting as quickly as possible to a signal. Attention and button timing also affect performance; match results are not a validated measure of real operational ability or proof of broad cognitive improvement.

Every round is labeled Time-sense Training, regardless of the target duration. Do not map targets to fictional missions or add an operational story. Reveal the target, ask the player to remember it, and use Start Timing and Stop Timing for the OK hints. Being early or late by the same amount has the same penalty.

Training briefing and target reveal lead into the timing challenge, sealed results and handover, a round-win celebration, the training report, and the match result. Use the same training vocabulary on the home screen, buttons, scenario previews, and both settlement screens. Keep the introduction short enough to read before timing; no extra cutscene or countdown interrupts an attempt. Single-player practice uses an AI Instructor in the same setting.

## Visuals and sound

The browser prototype follows the hand-crafted military pixel-art animation direction of Metal Slug rather than minimalist geometric arcade icons. Original red-headband and blue-beret agents, a sunset outpost, worn olive hardware, sand-gold typography, a rust-red OK button, share one visual direction. The browser screen now follows the firmware layout instead of adding CRT effects. Generated artwork and prompts are recorded in [the asset inventory](../assets/README.md#time-duel-artwork). Firmware uses size-budgeted derivatives of these assets, Chinese font subsets, and LVGL layouts. Preserve a non-overlapping top-right battery indicator; the browser's case shape and battery reading are illustrative.

- **During timing:** hold the agent's ready pose and keep the scene and scanlines stationary. Do not provide rhythmic visual or sound cues.
- **Round win:** switch the winner to a raised-fist pose, add a short impact burst, stamp in a ROUND WIN banner, and pulse the score. Use a neutral DRAW state without the burst for ties.
- **Match victory:** name the winner of this training match, show celebratory stars, preserve the final score, then return home before another match.
- **Animation fidelity:** the prototype uses two illustrated poses per agent plus stepped CSS transforms, not a finished frame-by-frame character animation set. Reduce-motion preferences suppress these transitions.
- **Memory:** ESP32-C3 has no PSRAM. The large source PNGs are browser-only design assets and are not linked into firmware. The device uses one opaque 240 x 240 RGB565 background and four transparent 80 x 112 RGB565A8 poses: 222,720 bytes of constant image data in Flash. Motion uses `lv_anim` transforms rather than full-screen frame sequences.
- **Sound:** original synthesized chiptune music plays outside timing. Accepted buttons, start, stop, round win/loss, and match victory have short cues. Entering timing stops music generation and interrupts the previous cue; only a one-shot 24 ms start cue remains, without a rhythmic soundtrack. Sound defaults on at volume 75/100; music-only PCM amplitudes are 1540 (melody) and 770 (bass), up 40% (about 2.9 dB) from the previous mix, while every cue waveform and the output-volume setting stay unchanged; DOWN on home toggles all sound. Idle dimming and exit stop music. A worker renders 16 kHz mono PCM in 128-sample chunks, never in input callbacks or the LVGL task. Acoustic latency and loudness require device listening. The browser synthesizes the same music and six cue PCM waveforms after a user gesture, without asserting equal speaker loudness.

## Single-player practice

The same device supports an AI opponent. The player takes their attempt, then the AI produces an independent estimate before settlement. Firmware and prototype use Gaussian error with `sigma = 0.12 * target`. A smaller coefficient means a more accurate, stronger opponent. Difficulty tiers remain subject to playtesting.

## Firmware implementation direction

- `main/duel_clock.c/h`: hardware-independent timing state machine, handover, judging, scoring, and AI model.
- `main/duel_ui.c`: shared-screen LVGL presentation and both celebration layers.
- `main/demo_duel.c`: page lifecycle, button events, audio dispatch, and demo registration.
- `main/duel_io.c/h`, `main/duel_sound.c/h`: worker-owned audio/battery I/O and a portable, allocation-free music/cue synthesizer.
- `tests/test_duel_sound.c`, `tests/duel_ui/`: music, mute, cue preemption, timing silence, real LVGL glyph/layout checks, static timing, and teardown coverage.
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

1. Recheck physical handover usability after removing the second confirmation.
2. Tune target range and AI difficulty after physical playtests.
3. Tune animation poses, cue duration, and visual density on the 240 × 320 display.
