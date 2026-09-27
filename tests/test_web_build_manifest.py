#!/usr/bin/env python3
"""Reject stale or incomplete web build receipts without requiring a Wasm SDK."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


class ManifestTest(unittest.TestCase):
    def check_receipt(self, game, mutate, valid=False):
        spec = importlib.util.spec_from_file_location(
            "web_builder", ROOT / f"tools/build_{game}_web.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as temp:
            module.ROOT = Path(temp)
            module.OUT = module.ROOT / "artifacts"
            module.OUT.mkdir()
            for source in module.SOURCES:
                path = module.ROOT / source
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"original source")
            artifacts = ["corridor.wasm", "presentation.json"] if game == "corridor" else ["duel.wasm"]
            for name in artifacts:
                (module.OUT / name).write_bytes(b"compiled artifact")
            receipt = {
                "sources": {p: module.digest(module.ROOT / p) for p in module.SOURCES},
                "artifacts": {p: module.digest(module.OUT / p) for p in artifacts},
            }
            mutate(module, receipt)
            (module.OUT / "manifest.json").write_text(json.dumps(receipt))
            with patch("sys.argv", ["builder", "--check"]), contextlib.redirect_stdout(io.StringIO()):
                if valid:
                    module.main()
                else:
                    with self.assertRaises((AssertionError, SystemExit, ValueError)):
                        module.main()

    def test_valid_receipts(self):
        for game in ("corridor", "duel"):
            with self.subTest(game=game):
                self.check_receipt(game, lambda m, r: None, valid=True)

    def test_empty_receipts_cannot_skip_validation(self):
        for game in ("corridor", "duel"):
            for field in ("sources", "artifacts"):
                with self.subTest(game=game, field=field):
                    self.check_receipt(game, lambda m, r: r.update({field: {}}))

    def test_source_change_requires_rebuild(self):
        for game in ("corridor", "duel"):
            with self.subTest(game=game):
                self.check_receipt(game, lambda m, r: (m.ROOT / m.SOURCES[0]).write_bytes(b"changed source"))

    def test_artifact_change_is_rejected(self):
        for game in ("corridor", "duel"):
            with self.subTest(game=game):
                self.check_receipt(game, lambda m, r: (m.OUT / next(iter(r["artifacts"]))).write_bytes(b"changed artifact"))


if __name__ == "__main__":
    unittest.main()
