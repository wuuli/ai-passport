<p align="right">
  <a href="RELEASE_TIME_CHALLENGE.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Time Challenge Release and Sharing

![Time Challenge cover](../assets/images/time-duel/time-challenge-cover.png)

## What it is

Time Challenge turns one AI Passport into a shared time-sense contest. Two players see the same target from 1.0 to 3.0 seconds, then take turns pressing OK to start and stop without a visible clock. The smaller error wins the round; the first to three wins the match.

The release includes pass-and-play and AI practice, sealed results, separate round and match celebrations, original arcade music, button feedback, and a live battery indicator.

## How to play

1. On the hardware menu, select Challenge and press OK.
2. On the game home screen, press UP to switch between two-player and AI practice, or DOWN to toggle sound.
3. Press OK to begin. Remember the target, press OK to start timing, and press OK again when the duration feels right.
4. In two-player mode, pass the device. The second player presses OK once to start and once to stop.
5. After both attempts are sealed, press OK to reveal the winner. The result page waits for OK before continuing.
6. Hold OK for one second to return to the hardware menu.

## Install the shared firmware

Use the merged `FoloToy-AI-Passport-full.bin`, not the application-only image.

1. Open [the AI Passport web flasher](https://ai-passport.folotoy.cn/tools/web-flasher/) in a supported browser.
2. Connect the AI Passport with a USB data cable.
3. Select `FoloToy-AI-Passport-full.bin`, choose a baud rate such as 460800, and flash it from address `0x0`.
4. Wait for write verification and restart before disconnecting the device.

The merged image ends below the protected identity and Recovery regions. It does not contain credentials, device identity, or a Recovery image. The current development board already lacked an installed Recovery image before this project; this firmware neither caused nor repairs that board-specific condition.

## Build from source

Activate ESP-IDF 5.5.3, then run:

```bash
./tools/validate.sh
```

The complete gate runs repository checks and host tests, builds the application, verifies the partition contract and 3 MB application limit, and produces `build/FoloToy-AI-Passport-full.bin` for flashing from `0x0`.

## Community publish profile

- Application name: `time-challenge`
- Chinese title: see the paired Simplified Chinese release guide
- English title: `Time Challenge`
- Cover: `assets/images/time-duel/time-challenge-cover.png`
- Firmware: `build/FoloToy-AI-Passport-full.bin`
- Source repository: <https://github.com/wuuli/ai-passport>; release branch: `feature/time-duel`

Chinese description: Turn one AI Passport into a two-player time-sense arena. Players share one device and take turns estimating a target duration entirely by feel; the smaller error wins, and the first to three wins the match. AI practice, separate round and match celebrations, original arcade music, and button feedback are included.

English description: Turn one AI Passport into a two-player time-sense showdown. Share the device, remember the target, then press OK to start and stop entirely by feel. The closer estimate wins each round, and the first to three takes the match. Includes AI practice, separate round and match celebrations, original arcade music, and responsive button cues.

The community upload also requires a fresh serial framebuffer capture and its matching publisher receipt. Preview and validation are safe preparation steps; upload happens only after the creator approves every displayed field.
