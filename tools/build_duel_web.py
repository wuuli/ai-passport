#!/usr/bin/env python3
"""Compile the firmware Time Duel clock state machine unchanged into a browser reactor module."""

import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "prototype/time-duel/firmware"
SOURCES = [
    "main/demo_duel.c",
    "main/duel_assets.c",
    "main/duel_clock.c",
    "main/duel_clock.h",
    "main/duel_font.c",
    "main/duel_title_font.c",
    "main/duel_ui.c",
    "tools/build_duel_web.py",
    "tools/duel_web_bridge.c",
]
EXPORTS = [
    "web_init",
    "web_handle",
    "web_key",
    "web_tick",
    "web_pause",
    "web_home",
    "web_exit",
    "web_set_fixture",
    "web_target_ms",
    "web_state",
]


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--sdk",
        default=os.environ.get("WASI_SDK_PATH"),
        help="Path to wasi-sdk root",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Verify shipped artifact/source hashes without a compiler",
    )
    args = parser.parse_args()

    OUT.mkdir(parents=True, exist_ok=True)
    manifest_path = OUT / "manifest.json"

    if args.check:
        if not manifest_path.exists():
            raise SystemExit(f"Missing manifest: {manifest_path}")
        m = json.loads(manifest_path.read_text())
        sources = m.get("sources", {})
        artifacts = m.get("artifacts", {})
        assert set(sources.keys()) == set(SOURCES), "Browser manifest source set mismatch"
        assert set(artifacts.keys()) == {"duel.wasm"}, "Browser manifest artifact set mismatch"
        for p, h in sources.items():
            actual = digest(ROOT / p)
            assert actual == h, f"Stale browser build: {p}"
        for p, h in artifacts.items():
            actual = digest(OUT / p)
            assert actual == h, f"Changed browser artifact: {p}"
        print("Time Duel browser source/artifact parity: PASS")
        return

    if not args.sdk:
        parser.error("Set WASI_SDK_PATH or --sdk to wasi-sdk")

    sdk = Path(args.sdk)
    clang = sdk / "bin/clang"
    if not clang.exists():
        parser.error(f"Clang not found at {clang}. Set --sdk or WASI_SDK_PATH")

    wasm_out = OUT / "duel.wasm"
    cmd = [
        str(clang),
        "-O2",
        "-ffp-contract=off",
        "-mexec-model=reactor",
        "-Imain",
        "-D_POSIX_C_SOURCE=200809L",
        "main/duel_clock.c",
        "tools/duel_web_bridge.c",
        "-lm",
        "-Wl,-z,stack-size=131072",
        "-Wl,--initial-memory=1048576",
        "-Wl,--max-memory=2097152",
        *[f"-Wl,--export={e}" for e in EXPORTS],
        "-o",
        str(wasm_out),
    ]
    subprocess.run(cmd, cwd=ROOT, check=True)

    compiler_version = subprocess.check_output([str(clang), "--version"], text=True).splitlines()[0]
    manifest = {
        "schema": 1,
        "compiler": compiler_version,
        "sources": {p: digest(ROOT / p) for p in sorted(SOURCES)},
        "artifacts": {
            "duel.wasm": digest(wasm_out),
        },
        "boundary": "Shared C duel_clock state machine; UI and asset sources tracked as visual/layout references. Browser UI/audio/input shell; browser timing is not device timing.",
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print("Built shared Time Duel browser module:", wasm_out.stat().st_size, "bytes")


if __name__ == "__main__":
    main()
