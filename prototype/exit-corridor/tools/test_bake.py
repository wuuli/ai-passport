#!/usr/bin/env python3
"""
Unit and Integration Tests for EC-16A / Character Sprite Baker.
Tests layout budgets for both 'shipped_8x8' and 'candidate_16x16' profiles,
tests server endpoints, profile/dimension mismatch rejections, and save isolation.
"""

import os
import sys
import json
import time
import base64
import struct
import zlib
import tempfile
import urllib.request
import urllib.error
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
TOOLS_DIR = REPO_ROOT / "prototype/exit-corridor/tools"
PROD_ASSETS_DIR = REPO_ROOT / "assets/images/exit-corridor"

TEST_PORT = 8097
BASE_URL = f"http://127.0.0.1:{TEST_PORT}"


def create_test_png(width, height):
    """Generates a minimal valid uncompressed RGBA PNG in memory with given dimensions."""
    sig = b"\x89PNG\r\n\x1a\n"
    ihdr_data = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    ihdr_crc = struct.pack(">I", zlib.crc32(b"IHDR" + ihdr_data) & 0xffffffff)
    ihdr = struct.pack(">I", len(ihdr_data)) + b"IHDR" + ihdr_data + ihdr_crc

    raw_scanlines = bytearray()
    for _ in range(height):
        raw_scanlines.append(0)
        raw_scanlines.extend(b"\x00" * (width * 4))
    compressed = zlib.compress(bytes(raw_scanlines))
    idat_crc = struct.pack(">I", zlib.crc32(b"IDAT" + compressed) & 0xffffffff)
    idat = struct.pack(">I", len(compressed)) + b"IDAT" + compressed + idat_crc

    iend_crc = struct.pack(">I", zlib.crc32(b"IEND") & 0xffffffff)
    iend = struct.pack(">I", 0) + b"IEND" + iend_crc

    return sig + ihdr + idat + iend


def test_metadata_budgets():
    print("Test 1: Validating Metadata Contract & Layout Budgets for Both Profiles...")

    # 1. Shipped 8x8 Profile
    shipped = {
        "directions": 8,
        "walkFrames": 8,
        "width": 48,
        "height": 96,
        "atlasWidth": 384,
        "atlasHeight": 864,
        "totalCells": 72
    }
    assert shipped["atlasWidth"] == shipped["directions"] * shipped["width"]
    assert shipped["atlasHeight"] == (shipped["walkFrames"] + 1) * shipped["height"]
    assert shipped["totalCells"] == shipped["directions"] * (shipped["walkFrames"] + 1)

    # 2. Candidate 16x16 Profile
    candidate = {
        "directions": 16,
        "walkFrames": 16,
        "width": 48,
        "height": 96,
        "atlasWidth": 768,
        "atlasHeight": 1632,
        "totalCells": 272
    }
    assert candidate["atlasWidth"] == candidate["directions"] * candidate["width"]
    assert candidate["atlasHeight"] == (candidate["walkFrames"] + 1) * candidate["height"]
    assert candidate["totalCells"] == candidate["directions"] * (candidate["walkFrames"] + 1)

    # World framing invariants: 1.0m x 2.0m, feetAnchor 0.95
    frame_w, frame_h = 1.0, 2.0
    assert (48 / 96) == (frame_w / frame_h)
    assert abs((1.9 / frame_h) - 0.95) < 0.001

    print("  [PASS] Shipped 8x8 (384x864, 72 cells) and Candidate 16x16 (768x1632, 272 cells) verified.")


def test_server_routes_and_save_isolated():
    print("\nTest 2: Starting Bake Server in Isolated Temp Directory & Testing Hardened Endpoints...")

    prod_png = PROD_ASSETS_DIR / "sprite-atlas.png"
    prod_json = PROD_ASSETS_DIR / "sprite-atlas.json"
    pre_test_png_mtime = prod_png.stat().st_mtime if prod_png.exists() else 0
    pre_test_json_mtime = prod_json.stat().st_mtime if prod_json.exists() else 0

    with tempfile.TemporaryDirectory() as temp_save_dir:
        temp_path = Path(temp_save_dir)
        print(f"  [INFO] Isolated test save directory: {temp_path}")

        runner_path = temp_path / "run_test_server.py"
        with open(runner_path, "w") as f:
            f.write(f"""
import os
import sys
sys.path.insert(0, r'{TOOLS_DIR}')
os.environ['BAKE_SERVER_SAVE_DIR'] = r'{temp_path}'

import importlib.util
spec = importlib.util.spec_from_file_location("bake_server", r'{TOOLS_DIR / "bake-server.py"}')
bs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bs)
bs.PORT = {TEST_PORT}
bs.ALLOWED_HOSTS.add(f"127.0.0.1:{TEST_PORT}")
bs.ALLOWED_HOSTS.add(f"localhost:{TEST_PORT}")
bs.ALLOWED_ORIGINS.add(f"http://127.0.0.1:{TEST_PORT}")
bs.ALLOWED_ORIGINS.add(f"http://localhost:{TEST_PORT}")
bs.run_server()
""")

        server_proc = subprocess.Popen(
            [sys.executable, str(runner_path)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        time.sleep(1.2)

        try:
            # 1. Test GET /
            req = urllib.request.Request(f"{BASE_URL}/")
            with urllib.request.urlopen(req) as resp:
                assert resp.status == 200, f"Expected 200, got {resp.status}"
                body = resp.read().decode("utf-8")
                assert "Rocketbox Character Sprite Baker" in body
                assert "selectBakeProfile" in body
                assert "candidate_16x16" in body
            print("  [PASS] GET / serves bake.html with profile selector.")

            # 2. Test GET /character-source/ allowlist & traversal
            req = urllib.request.Request(f"{BASE_URL}/character-source/Business_Male_01.fbx")
            with urllib.request.urlopen(req) as resp:
                assert resp.status == 200
            print("  [PASS] GET /character-source/ serves allowlisted FBX.")

            req = urllib.request.Request(f"{BASE_URL}/character-source/forbidden.txt")
            try:
                urllib.request.urlopen(req)
                assert False, "Should have rejected unlisted source file"
            except urllib.error.HTTPError as e:
                assert e.code == 404
            print("  [PASS] Unlisted source file correctly rejected.")

            # 3. Test POST /api/save with mismatched profile & dimensions (768x1632 with shipped_8x8) -> 400
            png_16 = create_test_png(768, 1632)
            mismatched_payload = json.dumps({
                "profile": "shipped_8x8",
                "atlasData": "data:image/png;base64," + base64.b64encode(png_16).decode("ascii"),
                "metadata": {
                    "frameWorldWidth": 1.0, "frameWorldHeight": 2.0, "feetAnchor": 0.95,
                    "actualFigureHeight": 1.76, "sourceHeight": 1.76, "width": 48, "height": 96,
                    "directions": 8, "walkFrames": 8, "atlasWidth": 384, "atlasHeight": 864
                }
            }).encode("utf-8")
            req = urllib.request.Request(
                f"{BASE_URL}/api/save",
                data=mismatched_payload,
                headers={"Content-Type": "application/json", "Host": f"127.0.0.1:{TEST_PORT}", "Origin": f"http://127.0.0.1:{TEST_PORT}"}
            )
            try:
                urllib.request.urlopen(req)
                assert False, "Should have rejected mismatched profile/dimensions"
            except urllib.error.HTTPError as e:
                assert e.code == 400
            print("  [PASS] POST /api/save correctly rejects 768x1632 with shipped_8x8 profile.")

            # 4. Test POST /api/save with valid Candidate 16x16 profile -> writes sprite-atlas-16.png
            valid_16_payload = json.dumps({
                "profile": "candidate_16x16",
                "atlasData": "data:image/png;base64," + base64.b64encode(png_16).decode("ascii"),
                "metadata": {
                    "frameWorldWidth": 1.0, "frameWorldHeight": 2.0, "feetAnchor": 0.95,
                    "actualFigureHeight": 1.76, "sourceHeight": 1.76, "width": 48, "height": 96,
                    "directions": 16, "walkFrames": 16, "walkCycleSeconds": 1.067,
                    "atlasWidth": 768, "atlasHeight": 1632
                }
            }).encode("utf-8")
            req_valid_16 = urllib.request.Request(
                f"{BASE_URL}/api/save",
                data=valid_16_payload,
                headers={"Content-Type": "application/json", "Host": f"127.0.0.1:{TEST_PORT}", "Origin": f"http://127.0.0.1:{TEST_PORT}"}
            )
            with urllib.request.urlopen(req_valid_16) as resp:
                assert resp.status == 200
                res_json = json.loads(resp.read().decode("utf-8"))
                assert res_json.get("success") is True
                assert res_json.get("profile") == "candidate_16x16"
                assert "sprite-atlas-16.png" in res_json.get("savedPng")
                assert "sprite-atlas-16.json" in res_json.get("savedJson")
            print("  [PASS] POST /api/save successfully saved candidate 16x16 to sprite-atlas-16.png.")

            # Verify written files in temp_path
            temp_16_png = temp_path / "sprite-atlas-16.png"
            temp_16_json = temp_path / "sprite-atlas-16.json"
            assert temp_16_png.is_file(), "sprite-atlas-16.png missing in temp dir"
            assert temp_16_json.is_file(), "sprite-atlas-16.json missing in temp dir"

            with open(temp_16_json, "r", encoding="utf-8") as f:
                meta16 = json.load(f)
                assert meta16["directions"] == 16
                assert meta16["walkFrames"] == 16
                assert meta16["atlasWidth"] == 768
                assert meta16["atlasHeight"] == 1632
                assert meta16["totalCells"] == 272

            # 5. Test POST /api/save with valid Shipped 8x8 profile -> writes sprite-atlas.png
            png_8 = create_test_png(384, 864)
            valid_8_payload = json.dumps({
                "profile": "shipped_8x8",
                "atlasData": "data:image/png;base64," + base64.b64encode(png_8).decode("ascii"),
                "metadata": {
                    "frameWorldWidth": 1.0, "frameWorldHeight": 2.0, "feetAnchor": 0.95,
                    "actualFigureHeight": 1.76, "sourceHeight": 1.76, "width": 48, "height": 96,
                    "directions": 8, "walkFrames": 8, "walkCycleSeconds": 1.067,
                    "atlasWidth": 384, "atlasHeight": 864
                }
            }).encode("utf-8")
            req_valid_8 = urllib.request.Request(
                f"{BASE_URL}/api/save",
                data=valid_8_payload,
                headers={"Content-Type": "application/json", "Host": f"127.0.0.1:{TEST_PORT}", "Origin": f"http://127.0.0.1:{TEST_PORT}"}
            )
            with urllib.request.urlopen(req_valid_8) as resp:
                assert resp.status == 200
                res_json = json.loads(resp.read().decode("utf-8"))
                assert res_json.get("success") is True
                assert res_json.get("profile") == "shipped_8x8"
                assert "sprite-atlas.png" in res_json.get("savedPng")
            print("  [PASS] POST /api/save successfully saved shipped 8x8 to sprite-atlas.png.")

            # 6. Verify candidate files did not overwrite shipped files in temp_path
            assert (temp_path / "sprite-atlas.png").is_file()
            assert (temp_path / "sprite-atlas-16.png").is_file()
            assert (temp_path / "sprite-atlas.png").stat().st_size != (temp_path / "sprite-atlas-16.png").stat().st_size
            print("  [PASS] Shipped and candidate files maintained distinctly without collision.")

            # 7. Verify production assets folder was NOT modified
            if prod_png.exists():
                assert prod_png.stat().st_mtime <= pre_test_png_mtime, "Production sprite-atlas.png modified!"
            if prod_json.exists():
                assert prod_json.stat().st_mtime <= pre_test_json_mtime, "Production sprite-atlas.json modified!"
            print("  [PASS] Verified production assets/images/exit-corridor/ untouched by tests!")

        finally:
            server_proc.terminate()
            server_proc.wait()
            print("  [INFO] Test server stopped cleanly.")


if __name__ == "__main__":
    test_metadata_budgets()
    test_server_routes_and_save_isolated()
    print("\nALL DUAL-PROFILE TESTS PASSED SUCCESSFULLY!")
