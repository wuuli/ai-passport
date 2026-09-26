<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Exit Corridor Character Sprite Atlas

The current browser and firmware share `commuter-device.bin` and the C decoder/renderer; see [web sync](../../../docs/WEB_PREVIEW_EXIT_CORRIDOR.md). The earlier JavaScript prototype runtime has been retired; character source assets, offline baking tools, and package validation tests are retained.

This directory contains the baked 2D character sprite atlas, metadata, and license documentation for the Exit Corridor commuter NPC.

## Files and layout

- `exit-corridor-promo-cover-v6.png`: the current 3:4 cover for community project 562 (`community-fb9b47f2`). It is promotional art rather than a device capture and is not loaded by the firmware or web renderer. [Edit prompt](promo-cover-v6-prompt.txt).
- `commuter-device.bin` / `.json`: default runtime ECSP package and metadata; **312,689 B**, 8 directions × (16 walk + 1 standing) = 136 frames.
- `sprite-atlas-16.png` / `.json`: offline 16-direction × 16-gait source, 768×1632, 272 cells, PNG **813,591 B**. Retained as the master offline bake source.
- `sprite-atlas.png` / `.json`: earlier 8-direction × 8-gait comparison, 384×864, 72 cells, PNG **219,522 B**. Retained as a historical comparison source.
- `ROCKETBOX_LICENSE.txt`: verbatim upstream MIT license.

All cells are 48×96. Direction 0 is front; positive angles view the person's right. The default selects source columns 0,2,4,...14 (45-degree steps) without mirroring or removing gait samples. Rows 0–15 contain a 1.067s walking cycle; row 16 is the resting pose. The full reference has 22.5-degree view steps.

World framing is 1×2m with a 1.76m figure and feet anchor 0.95: frame top Y=1.9m, bottom Y=-0.1m, soles Y=0. Tall anomalies stretch only Y. Viewing poses remain discretely sampled.

## Source Provenance

- **Source repository**: [Microsoft-Rocketbox](https://github.com/microsoft/Microsoft-Rocketbox)
- **Pinned commit**: `0943055db6ec570bcef9f2c8b41c9e5467c808f9`
- **License**: MIT License (see `ROCKETBOX_LICENSE.txt`). Copyright © 2020 Microsoft.
- **Avatar asset**: `Assets/Avatars/Professions/Business_Male_01/Export/Business_Male_01.fbx`
  - Runtime mesh: `m005_hipoly_81_bones_opacity` (22,305 vertices, 80 runtime bones).
  - Textures: `m005_body_color.tga` (2048 × 2048), `m005_head_color.tga` (2048 × 2048), `m005_opacity_color.tga` (1024 × 1024).
- **Walk animation**: `Assets/Animations/all_animations_max_motextr_xy/m_walk_neutral.max.fbx`
  - Root motion removed: forward translation on `Bip01.position` zeroed ($X=0, Z=0$) while preserving natural vertical hip bounce in $Y$.
- **Standing pose**: `Assets/Animations/all_animations_max_motextr_static/m_idle_neutral_01.max.fbx`
  - Sampled at $t = 0.5\text{s}$ for an authentic resting standing posture with arms relaxed and feet grounded (not derived from mid-walk).

## Storage and memory

The firmware reads ECSP from Flash and decodes one active frame into 4,608 B indexed pixels and a 576 B alpha mask. It never expands the full source atlas. The WebAssembly preview uses the same C decoder; the JavaScript decoder is retained only for offline package tests.

Selecting eight views while keeping all sixteen gait samples reduces the package from 622,522 B to 312,689 B. FBX, skeletons and PNG decoding belong to offline baking only. The earlier aggregate browser texture budget is retired; current resource accounting is in the [design record](../../../docs/RESEARCH_EXIT_CORRIDOR.md).

ECSP stores a 24-byte header, absolute frame offsets, per-frame bounding boxes, RGB565+alpha palettes and transparent scanline spans. Nonzero alpha above 8 is preserved; alpha 0–8 is discarded. More than 256 colors in a frame is rejected. Damaged headers, frame boundaries, palette indexes and truncated streams fail visibly before play.

## Reproduction

1. Download raw Rocketbox source assets into `/tmp/exit-corridor-character-source/`:
   ```bash
   mkdir -p /tmp/exit-corridor-character-source
   BASE="https://raw.githubusercontent.com/microsoft/Microsoft-Rocketbox/0943055db6ec570bcef9f2c8b41c9e5467c808f9"
   curl -sL "$BASE/Assets/Avatars/Professions/Business_Male_01/Export/Business_Male_01.fbx" -o /tmp/exit-corridor-character-source/Business_Male_01.fbx
   curl -sL "$BASE/Assets/Animations/all_animations_max_motextr_xy/m_walk_neutral.max.fbx" -o /tmp/exit-corridor-character-source/m_walk_neutral.max.fbx
   curl -sL "$BASE/Assets/Animations/all_animations_max_motextr_static/m_idle_neutral_01.max.fbx" -o /tmp/exit-corridor-character-source/m_idle_neutral_01.max.fbx
   curl -sL "$BASE/Assets/Avatars/Professions/Business_Male_01/Textures/m005_body_color.tga" -o /tmp/exit-corridor-character-source/m005_body_color.tga
   curl -sL "$BASE/Assets/Avatars/Professions/Business_Male_01/Textures/m005_head_color.tga" -o /tmp/exit-corridor-character-source/m005_head_color.tga
   curl -sL "$BASE/Assets/Avatars/Professions/Business_Male_01/Textures/m005_opacity_color.tga" -o /tmp/exit-corridor-character-source/m005_opacity_color.tga
   ```
2. Start the local bake server:
   ```bash
   python3 prototype/exit-corridor/tools/bake-server.py
   ```
3. Open `http://127.0.0.1:8099/tools/bake.html` in a WebGL-capable browser.
4. Select **candidate_16x16**, then click **Bake Sprite Atlas** to render 272 frames. The shipped_8x8 profile remains available and writes separate files.
5. Click **Save to Assets** to post the verified atlas and metadata to `assets/images/exit-corridor/`.

6. Generate the device package and its separate metadata from the saved source:

```bash
node prototype/exit-corridor/tools/pack-sprites.cjs \
  assets/images/exit-corridor/sprite-atlas-16.png \
  assets/images/exit-corridor/sprite-atlas-16.json \
  assets/images/exit-corridor/commuter-device.bin \
  --dir-step 2 --metadata-out assets/images/exit-corridor/commuter-device.json
node prototype/exit-corridor/packed-sprites.test.cjs
```

`device-validation/menu-20260919.png` decodes a read-only serial capture from the connected user device; its paired JSON records the original RGB565 payload hash. It is validation evidence and is not included in the game asset package.

`device-validation/menu-tool-20260919.json` and its paired PNG retain the reusable host tool's repeat capture and UTC timestamp, with payload SHA-256 matching the original capture. `serial-observation-20260919.json` records a silent 30.15-second observation window only; it does not establish stability.

`browser-validation/surfaces-20260919.json` records actual Canvas texture conversion: 14 surfaces, every exit number 0–8, maximum 103,512 B encoded data and 107,272 B retained typed arrays. This is browser evidence, excludes a future firmware container and is not device memory validation.

`browser-validation/flow-20260919.json` records actual browser input, boundary traversal, review recovery, eight-passage completion, replay and regression results. Controlled browser tests are distinct from user blind play and device acceptance.

`browser-validation/seams-20260919.json` records actual traversal of both reachable corner connectors, heading-preserving transfers, entry-relative decisions, 0→8 recovery and replay on both renderers. It includes source hashes and the corrected WebGL initialization failure; it is browser evidence only.

Seamless corridor acceptance: [browser record](browser-validation/seamless-20260919.json), including real GPU pixel comparisons, physical traversal and source hashes. This supersedes the earlier fade-based seam acceptance; it does not establish device performance.

Corner assistance and native portrait/landscape comparison: [browser record](browser-validation/corner-orientation-20260919.json), including 384-case seam audits and actual corner traversal; device performance and grip remain unverified.

Firmware embeds `commuter-device.bin` directly in application Flash. `main/corridor_palette.inc` stores a designed 256-color palette and lighting tables; the native framebuffer is 77,824 B including palette, with a 13,264 B renderer (split allocations, including a 576 B transparency mask) on the 64-bit host (target structure size is reported in device logs). `main/corridor_font.c` and `main/corridor_notice.inc` subset Noto Sans SC under [SIL OFL](../../fonts/time-duel/OFL.txt). Reproduce with `tools/generate_corridor_font.py`, `tools/generate_corridor_notice.py` (Pillow), and `tools/generate_corridor_palette.py`; a 65,536 B Flash-only RGB565-to-index lookup is generated by `tools/generate_corridor_sprite_lut.c`. Five RGB565 scanout rows and their palette require another 2,912 B static RAM; the font input and converter are the same Noto Sans SC / lv_font_conv 1.5.3 sources documented in the root assets index. The firmware palette/procedural surface port is distinct from WebGL reference rendering.

Walls use plain light-gray paint with depth lighting and skirting; transverse connector walls (`side >= 2`) dim by one shade for corner face contrast. Native rounded-corner, observation-centering, frame cadence and screenshot recovery evidence is indexed in the [device validation record](../../../docs/DEVICE_VALIDATION_EXIT_CORRIDOR.md).
