#!/usr/bin/env python3
"""
Hardened Bake Server for EC-16A / Character Sprite Atlas.
Supports both 'shipped_8x8' (384x864) and 'candidate_16x16' (768x1632) profiles.
Strictly validates dimensions and metadata against allowed profile targets,
never permitting candidate files to overwrite shipped 8x8 assets.
"""

import os
import sys
import json
import base64
import struct
from pathlib import Path
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse

PORT = 8099
HOST = "127.0.0.1"

REPO_ROOT = Path(__file__).resolve().parent.parent.parent.parent
TOOLS_DIR = (REPO_ROOT / "prototype/exit-corridor/tools").resolve()
VENDOR_DIR = (REPO_ROOT / "prototype/exit-corridor/vendor").resolve()
ASSETS_DIR = (REPO_ROOT / "assets").resolve()

custom_save_dir = os.environ.get("BAKE_SERVER_SAVE_DIR")
if custom_save_dir:
    TARGET_SAVE_DIR = Path(custom_save_dir).resolve()
else:
    TARGET_SAVE_DIR = (REPO_ROOT / "assets/images/exit-corridor").resolve()

RAW_SOURCE_DIR = Path("/tmp/exit-corridor-character-source").resolve()

MAX_PAYLOAD_BYTES = 6 * 1024 * 1024  # 6MB to accommodate candidate 16x16 PNG

ALLOWED_CHARACTER_SOURCES = {
    "Business_Male_01.fbx",
    "m_walk_neutral.max.fbx",
    "m_idle_neutral_01.max.fbx",
    "m005_body_color.tga",
    "m005_head_color.tga",
    "m005_opacity_color.tga"
}

ALLOWED_HOSTS = {f"127.0.0.1:{PORT}", f"localhost:{PORT}", f"127.0.0.1", f"localhost"}
ALLOWED_ORIGINS = {f"http://127.0.0.1:{PORT}", f"http://localhost:{PORT}"}


class BakeRequestHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        sys.stderr.write(f"[BakeServer] {args[0]} - {args[1]}\n")

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path

        if path in ("/", "/index.html", "/bake", "/bake.html"):
            return self.serve_file(TOOLS_DIR / "bake.html", "text/html; charset=utf-8")

        if path.startswith("/tools/"):
            rel = path[len("/tools/"):]
            return self.serve_safe(TOOLS_DIR, rel)

        if path.startswith("/vendor/"):
            rel = path[len("/vendor/"):]
            return self.serve_safe(VENDOR_DIR, rel)

        if path.startswith("/character-source/"):
            rel = path[len("/character-source/"):].strip("/")
            if rel not in ALLOWED_CHARACTER_SOURCES:
                self.send_error(404, f"Source file '{rel}' not allowlisted")
                return
            return self.serve_safe(RAW_SOURCE_DIR, rel)

        if path.startswith("/assets/"):
            rel = path[len("/assets/"):]
            return self.serve_safe(ASSETS_DIR, rel)

        self.send_error(404, "Not Found")

    def serve_safe(self, base_dir: Path, relative_path: str):
        try:
            full_path = (base_dir / relative_path).resolve()
            full_path.relative_to(base_dir)
        except (ValueError, RuntimeError):
            self.send_error(403, "Forbidden: Invalid path traversal")
            return

        if not full_path.is_file():
            self.send_error(404, "File Not Found")
            return

        suffix = full_path.suffix.lower()
        mime_map = {
            ".html": "text/html; charset=utf-8",
            ".js": "application/javascript; charset=utf-8",
            ".mjs": "application/javascript; charset=utf-8",
            ".css": "text/css; charset=utf-8",
            ".json": "application/json; charset=utf-8",
            ".png": "image/png",
            ".tga": "image/x-tga",
            ".fbx": "application/octet-stream",
            ".txt": "text/plain; charset=utf-8",
            ".md": "text/markdown; charset=utf-8"
        }
        mime = mime_map.get(suffix, "application/octet-stream")
        return self.serve_file(full_path, mime)

    def serve_file(self, file_path: Path, content_type: str):
        try:
            with open(file_path, "rb") as f:
                content = f.read()
            self.send_response(200)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(content)))
            self.end_headers()
            self.wfile.write(content)
        except Exception as e:
            self.send_error(500, f"Internal error: {e}")

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path != "/api/save":
            self.send_error(404, "Endpoint not found")
            return

        # 1. Enforce Host header
        host = self.headers.get("Host", "").strip()
        if host not in ALLOWED_HOSTS:
            self.send_error(403, f"Forbidden: Untrusted Host '{host}'")
            return

        # 2. Enforce Origin if provided
        origin = self.headers.get("Origin")
        if origin and origin.strip() not in ALLOWED_ORIGINS:
            self.send_error(403, f"Forbidden: Untrusted Origin '{origin}'")
            return

        # 3. Enforce Content-Type
        content_type = self.headers.get("Content-Type", "").strip()
        if not content_type.startswith("application/json"):
            self.send_error(415, "Unsupported Media Type: expected application/json")
            return

        # 4. Check Content-Length
        try:
            length = int(self.headers.get("Content-Length", 0))
        except ValueError:
            self.send_error(400, "Invalid Content-Length")
            return

        if length <= 0 or length > MAX_PAYLOAD_BYTES:
            self.send_error(413, f"Payload size exceeds limit: {length} bytes")
            return

        # 5. Parse JSON
        try:
            body = self.rfile.read(length)
            payload = json.loads(body.decode("utf-8"))
        except Exception as e:
            self.send_error(400, f"Invalid JSON body: {e}")
            return

        atlas_data = payload.get("atlasData")
        metadata = payload.get("metadata")
        profile = payload.get("profile")

        if not atlas_data or not isinstance(atlas_data, str):
            self.send_error(400, "Missing or invalid 'atlasData'")
            return

        # 6. Validate Base64
        if "base64," in atlas_data:
            atlas_data = atlas_data.split("base64,")[1]

        try:
            png_bytes = base64.b64decode(atlas_data, validate=True)
        except Exception as e:
            self.send_error(400, f"Failed to validate base64 PNG: {e}")
            return

        # 7. Validate PNG header (8 bytes)
        png_magic = b"\x89PNG\r\n\x1a\n"
        if len(png_bytes) < 24 or png_bytes[:8] != png_magic:
            self.send_error(400, "Invalid PNG file signature")
            return

        # 8. Read IHDR dimensions
        try:
            w, h = struct.unpack(">II", png_bytes[16:24])
        except Exception as e:
            self.send_error(400, f"Could not read PNG IHDR: {e}")
            return

        # 9. Profile resolution and strict dimension enforcement
        if not profile:
            if w == 768 and h == 1632:
                profile = "candidate_16x16"
            elif w == 384 and h == 864:
                profile = "shipped_8x8"
            else:
                self.send_error(400, f"Unknown atlas dimensions: {w}x{h}")
                return

        if profile == "shipped_8x8":
            expected_w, expected_h = 384, 864
            expected_dirs, expected_frames = 8, 8
            png_filename = "sprite-atlas.png"
            json_filename = "sprite-atlas.json"
        elif profile == "candidate_16x16":
            expected_w, expected_h = 768, 1632
            expected_dirs, expected_frames = 16, 16
            png_filename = "sprite-atlas-16.png"
            json_filename = "sprite-atlas-16.json"
        else:
            self.send_error(400, f"Invalid profile: '{profile}'")
            return

        # Strict profile/dimension mismatch rejection
        if w != expected_w or h != expected_h:
            self.send_error(400, f"Atlas dimensions {w}x{h} do not match profile '{profile}' (expected {expected_w}x{expected_h})")
            return

        # 10. Validate Metadata contract values
        if not isinstance(metadata, dict):
            self.send_error(400, "Missing or invalid metadata object")
            return

        required_meta_keys = [
            "frameWorldWidth", "frameWorldHeight", "feetAnchor", "actualFigureHeight",
            "sourceHeight", "width", "height", "directions", "walkFrames", "atlasWidth", "atlasHeight"
        ]
        for k in required_meta_keys:
            if k not in metadata:
                self.send_error(400, f"Missing required metadata key: '{k}'")
                return

        if metadata.get("width") != 48 or metadata.get("height") != 96:
            self.send_error(400, f"Invalid cell dims: expected 48x96, got {metadata.get('width')}x{metadata.get('height')}")
            return

        if metadata.get("atlasWidth") != expected_w or metadata.get("atlasHeight") != expected_h:
            self.send_error(400, f"Invalid metadata atlas dims for profile '{profile}': expected {expected_w}x{expected_h}, got {metadata.get('atlasWidth')}x{metadata.get('atlasHeight')}")
            return

        if metadata.get("directions") != expected_dirs or metadata.get("walkFrames") != expected_frames:
            self.send_error(400, f"Invalid directions/walkFrames for profile '{profile}': expected {expected_dirs}/{expected_frames}, got {metadata.get('directions')}/{metadata.get('walkFrames')}")
            return

        if float(metadata.get("actualFigureHeight", 0)) != 1.76 or float(metadata.get("sourceHeight", 0)) != 1.76:
            self.send_error(400, "actualFigureHeight and sourceHeight must be 1.76")
            return

        if float(metadata.get("frameWorldWidth", 0)) != 1.0 or float(metadata.get("frameWorldHeight", 0)) != 2.0:
            self.send_error(400, "frameWorldWidth must be 1.0 and frameWorldHeight must be 2.0")
            return

        if float(metadata.get("feetAnchor", 0)) != 0.95:
            self.send_error(400, "feetAnchor must be 0.95")
            return

        # 11. Strictly bounded write destination
        TARGET_SAVE_DIR.mkdir(parents=True, exist_ok=True)
        png_dest = TARGET_SAVE_DIR / png_filename
        json_dest = TARGET_SAVE_DIR / json_filename

        # Write PNG
        with open(png_dest, "wb") as f:
            f.write(png_bytes)

        # Write canonical JSON
        final_meta = {
            "profile": profile,
            "frameWorldWidth": 1.0,
            "frameWorldHeight": 2.0,
            "feetAnchor": 0.95,
            "actualFigureHeight": 1.76,
            "sourceHeight": 1.76,
            "width": 48,
            "height": 96,
            "directions": expected_dirs,
            "walkFrames": expected_frames,
            "walkCycleSeconds": float(metadata.get("walkCycleSeconds", 1.067)),
            "atlasWidth": expected_w,
            "atlasHeight": expected_h,
            "totalCells": expected_dirs * (expected_frames + 1),
            "license": "Microsoft Rocketbox (MIT License, Copyright 2020 Microsoft)",
            "licenseFile": "ROCKETBOX_LICENSE.txt",
            "sourceRepository": "https://github.com/microsoft/Microsoft-Rocketbox",
            "sourceRevision": "0943055db6ec570bcef9f2c8b41c9e5467c808f9",
            "sourceAvatar": "Business_Male_01",
            "sourceAnimation": "m_walk_neutral.max.fbx",
            "standingSource": "m_idle_neutral_01.max.fbx",
            "standingSampleTime": 0.5,
            "runtimeBones": 80,
            "runtimeVertices": 22305
        }

        with open(json_dest, "w", encoding="utf-8") as f:
            json.dump(final_meta, f, indent=2, ensure_ascii=False)

        try:
            rel_png = str(png_dest.relative_to(REPO_ROOT))
            rel_json = str(json_dest.relative_to(REPO_ROOT))
        except ValueError:
            rel_png = str(png_dest)
            rel_json = str(json_dest)

        resp = {
            "success": True,
            "profile": profile,
            "savedPng": rel_png,
            "savedJson": rel_json,
            "bytes": len(png_bytes),
            "dimensions": f"{w}x{h}",
            "metadata": final_meta
        }

        resp_bytes = json.dumps(resp).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(resp_bytes)))
        self.end_headers()
        self.wfile.write(resp_bytes)


def run_server():
    server_address = (HOST, PORT)
    httpd = HTTPServer(server_address, BakeRequestHandler)
    print(f"============================================================")
    print(f"EC-16A Character Sprite Bake Server running on http://{HOST}:{PORT}/")
    print(f"Bake Page URL: http://{HOST}:{PORT}/tools/bake.html")
    print(f"Save destination: {TARGET_SAVE_DIR}")
    print(f"Profiles: 'shipped_8x8' -> sprite-atlas.png, 'candidate_16x16' -> sprite-atlas-16.png")
    print(f"Security: Strictly 127.0.0.1, no wildcard CORS, Origin enforced.")
    print(f"============================================================")
    sys.stdout.flush()
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server...")
        httpd.server_close()


if __name__ == "__main__":
    run_server()
