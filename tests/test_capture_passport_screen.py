#!/usr/bin/env python3
"""Comprehensive test suite for tools/capture_passport_screen.py.

Tests:
1. Bit-exact RGB565LE to RGB888 decoding.
2. Pure-Python standard library PNG creation and scanline decompression.
3. Protocol framing, extreme fragmentation, and embedded binary newlines.
4. Bounded sliding window for leading logs and 256-byte header ceiling (with or without newline).
5. Sanitized error messages (no raw device text or tokens in exceptions).
6. Safe POSIX serial opening:
   * Baud validation before open.
   * termios.error and OSError cleanup on setup failure (verifying actual os.close and TIOCNXCL unlock).
   * EBUSY handling.
7. End-to-end PTY simulation for screenshot capture with shared total deadline.
8. Bounded listen mode with split-chunk keyword matching and duplicate count prevention.
9. Duration parameter validation (rejects NaN, Inf, non-positive, >300s) across all public functions.
"""

from __future__ import annotations

import errno
import fcntl
import hashlib
import json
import math
import os
import pty
import struct
import sys
import tempfile
import termios
import threading
import time
import unittest
import zlib
from unittest.mock import patch

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from tools.capture_passport_screen import (
    DEFAULT_BAUDRATE,
    PROTOCOL_COMMAND,
    encode_png,
    open_serial_posix,
    parse_screenshot_stream,
    rgb565le_to_rgb888,
    run_capture,
    run_listen,
    save_capture_outputs,
    send_command_nonblocking,
    validate_duration,
)


class TestRgb565Conversion(unittest.TestCase):
    """Tests bit-exact decoding of RGB565 little-endian pixels to RGB888."""

    def test_pure_primary_colors(self) -> None:
        raw = bytes([
            0x00, 0xF8,  # Red (0xF800 LE)
            0xE0, 0x07,  # Green (0x07E0 LE)
            0x1F, 0x00,  # Blue (0x001F LE)
            0xFF, 0xFF,  # White (0xFFFF)
            0x00, 0x00,  # Black (0x0000)
        ])
        rgb = rgb565le_to_rgb888(raw, 5, 1)
        expected = bytes([
            255, 0, 0,
            0, 255, 0,
            0, 0, 255,
            255, 255, 255,
            0, 0, 0,
        ])
        self.assertEqual(rgb, expected)

    def test_invalid_length_rejected(self) -> None:
        with self.assertRaises(ValueError):
            rgb565le_to_rgb888(b"\x00\x00\x00", 2, 1)


class TestPngEncoding(unittest.TestCase):
    """Tests standard library PNG creation and scanline decompression."""

    def test_valid_png_structure(self) -> None:
        width = 4
        height = 2
        rgb_data = bytes([255, 0, 0] * (width * height))
        png_bytes = encode_png(rgb_data, width, height)

        self.assertEqual(png_bytes[:8], b"\x89PNG\r\n\x1a\n")

        ihdr_len = struct.unpack(">I", png_bytes[8:12])[0]
        self.assertEqual(ihdr_len, 13)
        self.assertEqual(png_bytes[12:16], b"IHDR")
        w, h, bit_depth, color_type = struct.unpack(">IIBB", png_bytes[16:26])
        self.assertEqual(w, width)
        self.assertEqual(h, height)
        self.assertEqual(bit_depth, 8)
        self.assertEqual(color_type, 2)

        idat_pos = 12 + 4 + ihdr_len + 4
        idat_len = struct.unpack(">I", png_bytes[idat_pos:idat_pos + 4])[0]
        self.assertEqual(png_bytes[idat_pos + 4:idat_pos + 8], b"IDAT")
        compressed_data = png_bytes[idat_pos + 8:idat_pos + 8 + idat_len]
        decompressed = zlib.decompress(compressed_data)

        self.assertEqual(len(decompressed), height * (1 + width * 3))
        self.assertEqual(decompressed[0], 0)
        self.assertEqual(decompressed[13], 0)


class TestProtocolParser(unittest.TestCase):
    """Tests protocol framing, bounded buffers, sanitized errors, and timeouts."""

    def test_clean_response(self) -> None:
        width = 120
        height = 160
        expected_bytes = width * height * 2
        dummy_payload = bytes(i % 256 for i in range(expected_bytes))
        stream = b"FAP_SCREENSHOT_V1 120 160 RGB565LE 38400\n" + dummy_payload

        def reader(max_chunk: int, timeout: float) -> bytes:
            nonlocal stream
            chunk = stream[:max_chunk]
            stream = stream[max_chunk:]
            return chunk

        w, h, fmt, payload = parse_screenshot_stream(reader, timeout=1.0)
        self.assertEqual(w, 120)
        self.assertEqual(h, 160)
        self.assertEqual(fmt, "RGB565LE")
        self.assertEqual(len(payload), expected_bytes)
        self.assertEqual(payload, dummy_payload)

    def test_preceding_log_noise_and_crlf(self) -> None:
        width = 4
        height = 4
        payload = b"\xaa\x55" * (width * height)
        noise = b"I (1234) main: initializing display\r\nW (1235) wifi: station disconnected\r\n"
        header = b"FAP_SCREENSHOT_V1 4 4 RGB565LE 32\r\n"
        full = noise + header + payload

        def reader(max_chunk: int, timeout: float) -> bytes:
            nonlocal full
            chunk = full[:max_chunk]
            full = full[max_chunk:]
            return chunk

        w, h, fmt, out_payload = parse_screenshot_stream(reader, timeout=1.0)
        self.assertEqual(w, 4)
        self.assertEqual(h, 4)
        self.assertEqual(out_payload, payload)

    def test_extreme_fragmentation(self) -> None:
        width = 2
        height = 2
        payload = b"\x12\x34\x56\x78\x9a\xbc\xde\xf0"
        full = b"log\nFAP_SCREENSHOT_V1 2 2 RGB565LE 8\n" + payload

        pos = 0

        def reader(max_chunk: int, timeout: float) -> bytes:
            nonlocal pos
            if pos < len(full):
                byte = full[pos:pos + 1]
                pos += 1
                return byte
            return b""

        w, h, fmt, out_payload = parse_screenshot_stream(reader, timeout=1.0)
        self.assertEqual(w, 2)
        self.assertEqual(h, 2)
        self.assertEqual(out_payload, payload)

    def test_binary_payload_with_embedded_newlines_and_tokens(self) -> None:
        width = 4
        height = 4
        tricky_payload = (
            b"\n\r\x00\xff" +
            b"FAP_SCREENSHOT_V1 " +
            b"\n\n\x00\x01\x02\x03\x04\x05\x06\x07"
        )
        self.assertEqual(len(tricky_payload), 32)
        full = b"FAP_SCREENSHOT_V1 4 4 RGB565LE 32\n" + tricky_payload

        def reader(max_chunk: int, timeout: float) -> bytes:
            nonlocal full
            chunk = full[:max_chunk]
            full = full[max_chunk:]
            return chunk

        w, h, fmt, out_payload = parse_screenshot_stream(reader, timeout=1.0)
        self.assertEqual(out_payload, tricky_payload)

    def test_massive_leading_noise_bounded_buffer(self) -> None:
        width = 2
        height = 2
        payload = b"\x01\x02\x03\x04\x05\x06\x07\x08"
        massive_noise = b"W (100) driver: buffer overflow message warning\n" * 1000
        header = b"FAP_SCREENSHOT_V1 2 2 RGB565LE 8\n"
        full = massive_noise + header + payload

        def reader(max_chunk: int, timeout: float) -> bytes:
            nonlocal full
            chunk = full[:max_chunk]
            full = full[max_chunk:]
            return chunk

        w, h, fmt, out_payload = parse_screenshot_stream(reader, timeout=2.0)
        self.assertEqual(w, 2)
        self.assertEqual(out_payload, payload)

    def test_header_line_exceeding_256_bytes_without_newline_rejected(self) -> None:
        long_garbage = b"FAP_SCREENSHOT_V1 " + (b"A" * 300)

        def reader(max_chunk: int, timeout: float) -> bytes:
            nonlocal long_garbage
            chunk = long_garbage[:max_chunk]
            long_garbage = long_garbage[max_chunk:]
            return chunk

        with self.assertRaises(ValueError) as ctx:
            parse_screenshot_stream(reader, timeout=0.5)
        self.assertIn("256 bytes without a newline", str(ctx.exception))

    def test_header_line_exceeding_256_bytes_with_newline_rejected(self) -> None:
        long_with_nl = b"FAP_SCREENSHOT_V1 " + (b"B" * 300) + b"\n"

        def reader(max_chunk: int, timeout: float) -> bytes:
            nonlocal long_with_nl
            chunk = long_with_nl[:max_chunk]
            long_with_nl = long_with_nl[max_chunk:]
            return chunk

        with self.assertRaises(ValueError) as ctx:
            parse_screenshot_stream(reader, timeout=0.5)
        self.assertIn("exceeded 256 bytes", str(ctx.exception))

    def test_sanitized_error_messages(self) -> None:
        secret = "SUPER_SECRET_DEVICE_TOKEN_12345"
        bad_header = f"FAP_SCREENSHOT_V1 100 {secret} RGB565LE 38400\n".encode("ascii")

        def reader(max_chunk: int, timeout: float) -> bytes:
            nonlocal bad_header
            chunk = bad_header[:max_chunk]
            bad_header = bad_header[max_chunk:]
            return chunk

        with self.assertRaises(ValueError) as ctx:
            parse_screenshot_stream(reader, timeout=0.5)
        self.assertNotIn(secret, str(ctx.exception))

    def test_unsupported_format_sanitized_error(self) -> None:
        bad_fmt_header = b"FAP_SCREENSHOT_V1 120 160 UNKNOWN_SECRET_FMT 38400\n"

        def reader(max_chunk: int, timeout: float) -> bytes:
            nonlocal bad_fmt_header
            chunk = bad_fmt_header[:max_chunk]
            bad_fmt_header = bad_fmt_header[max_chunk:]
            return chunk

        with self.assertRaises(ValueError) as ctx:
            parse_screenshot_stream(reader, timeout=0.5)
        self.assertNotIn("UNKNOWN_SECRET_FMT", str(ctx.exception))
        self.assertIn("unsupported pixel format; expected 'RGB565LE'", str(ctx.exception))

    def test_malformed_headers_rejected(self) -> None:
        bad_cases = [
            b"FAP_SCREENSHOT_V1 0 160 RGB565LE 0\n",
            b"FAP_SCREENSHOT_V1 -10 160 RGB565LE 100\n",
            b"FAP_SCREENSHOT_V1 120 160 RGB888 57600\n",
            b"FAP_SCREENSHOT_V1 120 160 RGB565LE 38401\n",
            b"FAP_SCREENSHOT_V1 2000 2000 RGB565LE 8000000\n",
            b"FAP_SCREENSHOT_V1 not_an_int 160 RGB565LE 38400\n",
            b"FAP_SCREENSHOT_V1 120 160\n",
        ]
        for case in bad_cases:
            def make_reader(stream: bytes):
                data = stream
                def _r(chunk: int, timeout: float):
                    nonlocal data
                    c = data[:chunk]
                    data = data[chunk:]
                    return c
                return _r

            with self.assertRaises(ValueError, msg=f"Should reject: {case!r}"):
                parse_screenshot_stream(make_reader(case), timeout=0.2)

    def test_timeout_raised(self) -> None:
        def empty_reader(max_chunk: int, timeout: float) -> bytes:
            time.sleep(0.02)
            return b""

        with self.assertRaises(TimeoutError):
            parse_screenshot_stream(empty_reader, timeout=0.08)

    def test_invalid_duration_in_public_functions(self) -> None:
        def dummy_reader(chunk: int, to: float) -> bytes:
            return b""

        for bad in [float("nan"), float("inf"), 0, -1.0, "5.0"]:
            with self.assertRaises((ValueError, TypeError)):
                parse_screenshot_stream(dummy_reader, timeout=bad)
            with self.assertRaises((ValueError, TypeError)):
                send_command_nonblocking(999, b"test", timeout=bad)


class TestDurationValidation(unittest.TestCase):
    """Tests validation of finite positive timeout and listen durations."""

    def test_valid_durations(self) -> None:
        self.assertEqual(validate_duration("timeout", 5.0), 5.0)
        self.assertEqual(validate_duration("listen_seconds", 10), 10.0)

    def test_invalid_durations_rejected(self) -> None:
        for val in [float("nan"), float("inf"), float("-inf"), 0, -5.0, None, "10", 301.0]:
            with self.assertRaises((ValueError, TypeError), msg=f"Should reject {val!r}"):
                validate_duration("duration", val)


class TestSaveCaptureOutputs(unittest.TestCase):
    """Tests file writing, overwrite protection, and sanitized JSON schema."""

    def test_output_generation(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            prefix = os.path.join(tmpdir, "test_cap")
            width = 4
            height = 2
            raw_bytes = bytes([0x1F, 0x00] * (width * height))

            res = save_capture_outputs(
                output_prefix=prefix,
                width=width,
                height=height,
                fmt="RGB565LE",
                raw_bytes=raw_bytes,
                port="/dev/mock_port",
                baudrate=115200
            )

            self.assertTrue(os.path.isfile(res["raw"]))
            self.assertTrue(os.path.isfile(res["png"]))
            self.assertTrue(os.path.isfile(res["json"]))

            with open(res["raw"], "rb") as f:
                self.assertEqual(f.read(), raw_bytes)

            with open(res["json"], "r", encoding="utf-8") as f:
                meta = json.load(f)

            self.assertEqual(meta["width"], 4)
            self.assertEqual(meta["height"], 2)
            self.assertEqual(meta["format"], "RGB565LE")
            self.assertEqual(meta["bytes"], len(raw_bytes))
            self.assertEqual(meta["port"], "/dev/mock_port")
            self.assertEqual(meta["baudrate"], 115200)

            expected_sha = hashlib.sha256(raw_bytes).hexdigest()
            self.assertEqual(meta["raw_sha256"], expected_sha)
            self.assertEqual(res["sha256"], expected_sha)

            with self.assertRaises(FileExistsError):
                save_capture_outputs(prefix, width, height, "RGB565LE", raw_bytes, "/dev/mock_port", 115200, overwrite=False)

            res_over = save_capture_outputs(prefix, width, height, "RGB565LE", raw_bytes, "/dev/mock_port", 115200, overwrite=True)
            self.assertEqual(res_over["sha256"], expected_sha)


class TestSerialSafetyAndCleanup(unittest.TestCase):
    """Tests serial cleanup, baudrate pre-validation, termios.error handling, and EBUSY locking."""

    def test_baudrate_validated_before_open(self) -> None:
        with self.assertRaises(ValueError) as ctx:
            open_serial_posix("/dev/non_existent_path_12345", 999999)
        self.assertIn("Unsupported baudrate", str(ctx.exception))

    def test_open_failure_cleans_up_descriptor_and_locks_on_termios_error(self) -> None:
        master_fd, slave_fd = pty.openpty()
        slave_name = os.ttyname(slave_fd)

        closed_fds = []
        real_close = os.close

        def close_spy(fd):
            closed_fds.append(fd)
            real_close(fd)

        ioctl_calls = []
        real_ioctl = fcntl.ioctl

        def ioctl_spy(fd, op, *args):
            ioctl_calls.append(op)
            return real_ioctl(fd, op, *args)

        # Mock tcsetattr to raise termios.error (which is NOT an OSError subclass on macOS CPython!)
        with patch("termios.tcsetattr", side_effect=termios.error("Simulated termios failure")):
            with patch("os.close", side_effect=close_spy):
                with patch("fcntl.ioctl", side_effect=ioctl_spy):
                    with self.assertRaises(termios.error):
                        open_serial_posix(slave_name, 115200)

        # Assert: descriptor was closed in cleanup despite termios.error
        self.assertTrue(len(closed_fds) >= 1, "os.close must be called during setup cleanup")

        # Assert: TIOCNXCL was called to release exclusive lock if TIOCEXCL was called
        if hasattr(termios, "TIOCEXCL") and hasattr(termios, "TIOCNXCL"):
            self.assertIn(termios.TIOCEXCL, ioctl_calls)
            self.assertIn(termios.TIOCNXCL, ioctl_calls)

        real_close(master_fd)
        real_close(slave_fd)

    def test_ebusy_raises_cleanly(self) -> None:
        master_fd, slave_fd = pty.openpty()
        slave_name = os.ttyname(slave_fd)

        with patch("fcntl.ioctl", side_effect=OSError(errno.EBUSY, "Device busy")):
            with self.assertRaises(OSError) as ctx:
                open_serial_posix(slave_name, 115200)
            self.assertIn("busy / locked", str(ctx.exception))

        os.close(master_fd)
        os.close(slave_fd)


class TestPtySimulation(unittest.TestCase):
    """Tests end-to-end serial interaction using POSIX pseudo-terminals (no real hardware)."""

    def test_run_capture_over_pty(self) -> None:
        master_fd, slave_fd = pty.openpty()
        slave_name = os.ttyname(slave_fd)

        width = 120
        height = 160
        payload_bytes = width * height * 2
        test_payload = bytes(x % 256 for x in range(payload_bytes))
        header = f"FAP_SCREENSHOT_V1 {width} {height} RGB565LE {payload_bytes}\n".encode("ascii")

        def simulator_worker():
            try:
                cmd = os.read(master_fd, 64)
                if PROTOCOL_COMMAND.strip() in cmd:
                    os.write(master_fd, b"I (100) boot: start\r\n")
                    os.write(master_fd, header)
                    chunk_size = 1024
                    for i in range(0, len(test_payload), chunk_size):
                        os.write(master_fd, test_payload[i:i + chunk_size])
            except OSError:
                pass

        sim_thread = threading.Thread(target=simulator_worker, daemon=True)
        sim_thread.start()

        with tempfile.TemporaryDirectory() as tmpdir:
            prefix = os.path.join(tmpdir, "pty_capture")
            res = run_capture(
                port=slave_name,
                output_prefix=prefix,
                baudrate=115200,
                timeout=3.0,
                overwrite=True
            )

            self.assertTrue(os.path.isfile(res["png"]))
            with open(res["raw"], "rb") as f:
                self.assertEqual(f.read(), test_payload)

        sim_thread.join(timeout=1.0)
        os.close(master_fd)
        os.close(slave_fd)

    def test_run_listen_mode_chunk_split_and_no_duplicate_counting(self) -> None:
        master_fd, slave_fd = pty.openpty()
        slave_name = os.ttyname(slave_fd)

        def log_emitter():
            try:
                time.sleep(0.04)
                os.write(master_fd, b"I (10) log: Guru Med")
                time.sleep(0.02)
                os.write(master_fd, b"itation error trigger\n")
                time.sleep(0.02)

                os.write(master_fd, b"I (20) normal log noise line 1\n")
                time.sleep(0.02)
                os.write(master_fd, b"I (30) normal log noise line 2\n")
                time.sleep(0.02)

                os.write(master_fd, b"W (40) WDT fired\nassertion failed: x > 0\n")
            except OSError:
                pass

        emitter_thread = threading.Thread(target=log_emitter, daemon=True)
        emitter_thread.start()

        with tempfile.TemporaryDirectory() as tmpdir:
            out_prefix = os.path.join(tmpdir, "listen_test")
            summary = run_listen(
                port=slave_name,
                listen_seconds=0.35,
                output_prefix=out_prefix,
                baudrate=115200,
                overwrite=True
            )

            matches = summary["keyword_matches"]
            self.assertEqual(matches["guru_meditation"], 1)
            self.assertEqual(matches["watchdog"], 1)
            self.assertEqual(matches["assertion_failure"], 1)
            self.assertEqual(matches["reboot"], 0)

            self.assertTrue(os.path.isfile(f"{out_prefix}.json"))

        emitter_thread.join(timeout=1.0)
        os.close(master_fd)
        os.close(slave_fd)


if __name__ == "__main__":
    unittest.main()
