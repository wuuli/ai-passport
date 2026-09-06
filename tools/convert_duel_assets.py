#!/usr/bin/env python3

import argparse
import re
import struct
import subprocess
from pathlib import Path

from PIL import Image, ImageOps


ROOT = Path(__file__).resolve().parent.parent


def descriptor(name, image):
    pixels = bytearray()
    for red, green, blue in image.convert("RGB").getdata():
        pixels.extend(struct.pack("<H", (red >> 3) << 11 | (green >> 2) << 5 | (blue >> 3)))
    rows = [", ".join(f"0x{value:02x}" for value in pixels[offset:offset + 24])
            for offset in range(0, len(pixels), 24)]
    source = f"static const uint8_t {name}_data[] = {{\n    " + ",\n    ".join(rows) + "\n};\n"
    source += f"""
const lv_image_dsc_t {name} = {{
    .header = {{.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
               .w = {image.width}, .h = {image.height}, .stride = {image.width * 2}}},
    .data_size = sizeof({name}_data),
    .data = {name}_data,
}};
"""
    return source, len(pixels)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--font-converter", type=Path, required=True)
    options = parser.parse_args()
    artwork = ROOT / "assets/images/time-duel"
    sheet = Image.open(artwork / "time-challenge-operatives.png")
    background = ImageOps.fit(Image.open(artwork / "time-challenge-outpost.png"), (240, 240),
                              method=Image.Resampling.NEAREST)
    images = [("duel_outpost", background)]
    crops = [("duel_agent_0", (150, 15, 560, 590)),
             ("duel_agent_1", (710, 15, 1100, 590)),
             ("duel_agent_0_win", (145, 594, 550, 1220)),
             ("duel_agent_1_win", (700, 594, 1095, 1220))]
    for name, bounds in crops:
        pose = ImageOps.pad(sheet.crop(bounds), (80, 112), method=Image.Resampling.NEAREST,
                            color=(34, 40, 29))
        images.append((name, pose))
    sources = ['#include "duel_assets.h"\n']
    image_bytes = 0
    for name, image in images:
        source, size = descriptor(name, image)
        sources.append(source)
        image_bytes += size
    (ROOT / "main/duel_assets.c").write_text("\n".join(sources))
    declarations = "\n".join(f"extern const lv_image_dsc_t {name};" for name, _ in images)
    (ROOT / "main/duel_assets.h").write_text(
        '#pragma once\n\n#include "lvgl.h"\n\n' + declarations +
        "\nextern const lv_font_t duel_font;\nextern const lv_font_t duel_title_font;\n")
    ui_source = (ROOT / "main/duel_ui.c").read_text()
    symbols = "".join(sorted(set(re.findall(r"[^\x00-\x7f]", ui_source))))
    for name, size, characters, ascii_range in [
        ("duel_font", 16, symbols, ["-r", "32-126"]),
        ("duel_title_font", 26, "掐秒挑战训练优胜", []),
    ]:
        output = ROOT / f"main/{name}.c"
        subprocess.run([str(options.font_converter), "--size", str(size), "--bpp", "2",
                        "--format", "lvgl", "--no-compress", "--no-kerning",
                        "--lv-include", "lvgl.h", "--font", str(options.font),
                        "--symbols", characters, *ascii_range, "-o", str(output)], check=True)
        source = re.sub(r"/\*.*?\*/", "", output.read_text(), flags=re.DOTALL)
        source = "\n".join(line.rstrip() for line in source.splitlines()) + "\n"
        output.write_text(re.sub(r"\n{3,}", "\n\n", source))
    print(f"RGB565 images: {image_bytes} bytes in Flash; CJK copy set: {len(symbols)} glyphs")


if __name__ == "__main__":
    main()
