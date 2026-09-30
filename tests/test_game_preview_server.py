#!/usr/bin/env python3
"""The game preview server exposes public game files, never the checkout root."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("preview", ROOT / "tools/serve_corridor_web.py")
PREVIEW = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREVIEW)


class PreviewRoutesTest(unittest.TestCase):
    def resolve(self, url):
        return Path(PREVIEW.Handler.translate_path(None, url))

    def test_game_entries_and_artifacts(self):
        for url, relative in {
            "/": "prototype/exit-corridor.html",
            "/time-duel-v2.html": "prototype/time-duel-v2.html",
            "/time-duel/firmware/duel.wasm": "prototype/time-duel/firmware/duel.wasm",
            "/exit-corridor/firmware/corridor.wasm": "prototype/exit-corridor/firmware/corridor.wasm",
            "/assets/images/time-duel/device/duel_agent_0.png": "assets/images/time-duel/device/duel_agent_0.png",
        }.items():
            with self.subTest(url=url):
                self.assertEqual(self.resolve(url), ROOT / relative)

    def test_private_paths_and_traversal_are_not_served(self):
        for url in ("/games.html", "/.git/config", "/sdkconfig", "/build/FoloToy-AI-Passport-full.bin",
                    "/time-duel/../../.git/config", "/exit-corridor/%2e%2e/%2e%2e/sdkconfig",
                    "/assets/images/time-duel/../../../README.md"):
            with self.subTest(url=url):
                self.assertEqual(self.resolve(url), ROOT / "__not_a_served_path__")


if __name__ == "__main__":
    unittest.main()
