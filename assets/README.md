<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

## Images

Store reusable source images and generated display assets in `images/`.

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

See also the [Time Duel artwork](#time-duel-artwork) and its firmware derivatives below.

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
- Time Challenge uses original procedural music and six cues in `main/duel_sound.c`, under the repository code license, with no imported recording or commercial soundtrack. The worker renders 16 kHz, 16-bit mono PCM in 128-sample blocks; no audio conversion or binary asset is required. Music is disabled during active timing.

## Time Duel Artwork

- `images/time-duel/time-challenge-outpost.png`: 1536 × 1024 opaque RGB PNG; original sunset military training base with a radio truck and bunker.
- `images/time-duel/time-challenge-operatives.png`: 1254 × 1254 opaque RGB PNG on a dark olive matte; a 2 × 2 character sheet. Top row: ready poses; bottom row: raised-fist victory poses. Left: red-headband agent; right: blue-beret agent.
- `images/time-duel/time-challenge-operatives-alpha.png`: reproducible RGBA derivative of the character sheet. `tools/convert_duel_assets.py` flood-fills only the edge-connected matte into transparent pixels before cropping the device poses.
- `images/time-duel/time-challenge-cover.png`: 1086 × 1448 exact-3:4 opaque RGB PNG; community cover showing the two original agents in a friendly timing duel at the sunset outpost. It was generated with the built-in OpenAI image-generation tool from the two source images above as character/style references, with no logo, text, weapon, or third-party source asset.
- Integration: `prototype/time-duel-military.css` loads these repository-local images as scenery for `prototype/time-duel-v2.html`; positioned character layers use transparent device derivatives so the agents blend with the outpost instead of showing a rectangular matte. The original generated source images remain unchanged.
- Source/license: generated for this prototype and release using the built-in OpenAI image-generation tool, not extracted from a commercial game's assets. No third-party source asset license is attached. [Generation and final-edit prompts](images/time-duel/generation-prompts.txt) are retained for reproduction.
- Browser device derivatives: `images/time-duel/device/*.png` are decoded directly from the RGB565 background and four RGB565A8 agent arrays in `main/duel_assets.c` with `python3 tools/export_duel_preview_assets.py` (Python standard library only). The positioned browser screen uses these 240 x 240 / 80 x 112 images without sprite-sheet masking; the review shell retains the original artwork. These PNGs add no firmware payload.
- Firmware derivatives: `main/duel_assets.c` contains one 240 x 240 opaque RGB565 background and four 80 x 112 transparent RGB565A8 ready/victory poses (222,720 bytes total in Flash). The large source PNGs themselves are not linked into firmware. The added Alpha planes remove the rectangular character matte while keeping the assets within the application budget.
- Chinese glyph subsets: `main/duel_font.c` (16 px copy and ASCII) and `main/duel_title_font.c` (26 px title) use Noto Sans SC, licensed under [SIL OFL 1.1](fonts/time-duel/OFL.txt). Source: [Noto Sans SC Regular](https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf), SHA-256 `faa6c9df652116dde789d351359f3d7e5d2285a2b2a1f04a2d7244df706d5ea9`. The full source font is a build-time input, not a runtime dependency.
- Reproduce with Python + Pillow 11.3.0 and `lv_font_conv` 1.5.3: `python tools/convert_duel_assets.py --font /path/to/NotoSansSC-Regular.otf --font-converter /path/to/lv_font_conv`. This preserves source PNGs and regenerates image descriptors and both font subsets from the UI copy. Re-run after adding Chinese copy. Firmware image data is read from Flash; actual peak RAM and screen fidelity still require device validation.

## Exit Corridor Character Artwork

- `images/exit-corridor/commuter-device.bin` / `.json`: default 8 directions × 16 gait frames + standing row, 48×96 cells, 312,689 B (305.36 KiB), decoded per frame on demand.
- `images/exit-corridor/sprite-atlas-16.png` / `.json`: 768×1632 offline source (813,591 B), retaining all 16 views for baking and packing; earlier `sprite-atlas.png` / `.json` remain packing test fixtures.
- `images/exit-corridor/ROCKETBOX_LICENSE.txt`: Microsoft Rocketbox MIT license (Copyright 2020 Microsoft), pinned revision `0943055db6ec570bcef9f2c8b41c9e5467c808f9`; Business_Male_01, neutral walk and idle are used only for offline baking.
- World frame 1×2m, figure height 1.76m, feet anchor 0.95; 16 gait samples per 1.067s cycle. The firmware links `commuter-device.bin` directly into Flash via `target_add_binary_data`, and the C renderer decodes only the single active frame on demand into RAM (`r->frame` 4,608 B + `r->alpha` 576 B).
- See the [character asset guide](images/exit-corridor/README.md) for provenance, format and reproducible commands.

The current corridor web preview directly uses the firmware asset package and C renderer. The generated `main/corridor_font.c` (16 px) and `main/corridor_title_font.c` (30 px, only the four title glyphs) use the same Noto Sans SC source and [OFL license](fonts/time-duel/OFL.txt) as the duel fonts above. Reproduce with `tools/generate_corridor_font.py --font /path/to/NotoSansSC-Regular.otf --converter /path/to/lv_font_conv`; add `--title` for the title subset. Both are Flash-resident font data, not new framebuffers. See [web sync](../docs/WEB_PREVIEW_EXIT_CORRIDOR.md) for shared sources, rebuilding and boundaries.
