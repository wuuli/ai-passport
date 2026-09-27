#!/usr/bin/env python3

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
SOURCE = (ROOT / "main/duel_assets.c").read_text()
ASSETS = {
    "duel_outpost": (240, 240, "LV_COLOR_FORMAT_RGB565"),
    "duel_agent_0": (80, 112, "LV_COLOR_FORMAT_RGB565A8"),
    "duel_agent_1": (80, 112, "LV_COLOR_FORMAT_RGB565A8"),
    "duel_agent_0_win": (80, 112, "LV_COLOR_FORMAT_RGB565A8"),
    "duel_agent_1_win": (80, 112, "LV_COLOR_FORMAT_RGB565A8"),
}


def asset_bytes(name):
    match = re.search(rf"{name}_data\[\] = \{{(.*?)\}};", SOURCE, re.S)
    if match is None:
        raise AssertionError(f"missing image array: {name}")
    return bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", match.group(1)))


def asset_descriptor(name):
    match = re.search(
        rf"const lv_image_dsc_t {name} = \{{(.*?)\n\}};", SOURCE, re.S
    )
    if match is None:
        raise AssertionError(f"missing image descriptor: {name}")
    return match.group(1)


def sparse_cmap_chars(source):
    chars = set()
    for match in re.finditer(
        r"\{[^{}]*\.range_start\s*=\s*(\d+)[^{}]*\.unicode_list\s*=\s*(unicode_list_\d+)[^{}]*\}",
        source,
        re.S,
    ):
        range_start = int(match.group(1))
        list_name = match.group(2)
        list_match = re.search(rf"{list_name}\[\]\s*=\s*\{{(.*?)\}};", source, re.S)
        if list_match:
            offsets = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]+)", list_match.group(1))]
            chars.update(chr(range_start + offset) for offset in offsets)
    return chars


class DuelAssetTest(unittest.TestCase):
    def test_color_formats_and_data_sizes(self):
        for name, (width, height, color_format) in ASSETS.items():
            with self.subTest(name=name):
                descriptor = asset_descriptor(name)
                self.assertIn(f".cf = {color_format}", descriptor)
                self.assertIn(f".w = {width}, .h = {height}, .stride = {width * 2}", descriptor)
                bytes_per_pixel = 3 if color_format.endswith("RGB565A8") else 2
                self.assertEqual(len(asset_bytes(name)), width * height * bytes_per_pixel)

    def test_agent_alpha_planes_have_visible_and_transparent_pixels(self):
        for name, (width, height, color_format) in ASSETS.items():
            if color_format != "LV_COLOR_FORMAT_RGB565A8":
                continue
            with self.subTest(name=name):
                alpha = asset_bytes(name)[width * height * 2:]
                self.assertIn(0, alpha)
                self.assertIn(255, alpha)

    def test_duel_font_covers_all_ui_glyphs(self):
        font_source = (ROOT / "main/duel_font.c").read_text()
        ui_source = (ROOT / "main/duel_ui.c").read_text()
        font_chars = sparse_cmap_chars(font_source)
        ui_chars = set(c for c in ui_source if ord(c) > 127)
        missing = ui_chars - font_chars
        self.assertEqual(missing, set(), f"duel_font missing glyphs from duel_ui.c: {missing}")

    def test_duel_title_font_covers_title_glyphs(self):
        title_font_source = (ROOT / "main/duel_title_font.c").read_text()
        ui_source = (ROOT / "main/duel_ui.c").read_text()
        title_chars = sparse_cmap_chars(title_font_source)
        expected = set("掐秒挑战训练优胜")
        self.assertTrue(expected.issubset(title_chars), "duel_title_font missing required title glyphs")

        for match in re.finditer(r'label\([^,]+,\s*"([^"]+)"[^)]*&duel_title_font', ui_source):
            label_text = match.group(1)
            label_non_ascii = set(c for c in label_text if ord(c) > 127)
            self.assertTrue(
                label_non_ascii.issubset(title_chars),
                f"duel_title_font missing glyphs for title label {label_text}",
            )


if __name__ == "__main__":
    unittest.main()
