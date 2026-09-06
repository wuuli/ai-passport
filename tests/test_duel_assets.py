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


if __name__ == "__main__":
    unittest.main()
