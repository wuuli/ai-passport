#!/usr/bin/env python3
"""POSIX device screenshot and passive serial listener for FoloToy AI Passport.

Zero-dependency standard library tool:
- Target: Espressif native USB Serial/JTAG on ESP32-C3 (e.g., /dev/cu.usbmodem2101).
- Non-destructive: opens via POSIX termios without issuing DTR/RTS ioctls.
  Clears HUPCL on active configuration. Note: restoring original termios on exit
  restores original HUPCL; driver behavior across generic external bridges varies.
- Exclusive locking via fcntl/TIOCEXCL; fails fast on EBUSY if device is in use.
- Robust cleanup: catches both OSError and termios.error during restore, and uses
  a strict finally block to guarantee os.close(fd) is always executed.
- Implements the FAP_SCREENSHOT_V1 protocol (main/fap_screenshot.c):
    Host sends: b"FAP_SCREENSHOT_V1\n"
    Device responds: b"FAP_SCREENSHOT_V1 <width> <height> RGB565LE <bytes>\n" + exact payload
- Shared capture deadline: command transmission and response reception share one total timeout.
- Bounded sliding window: limits buffer size during leading logs, limits header line
  to 256 bytes (with or without newline), never leaks raw device/serial text into exceptions.
- Non-blocking command transmission handling EAGAIN and write deadlines.
- Generates raw RGB565LE binary, pure-Python standard-library PNG, and sanitized JSON metadata.
- Safe output defaults: writes to a secure temporary prefix by default, never polluting repo root.
- Passive listen mode (--listen-seconds): monitors stream for crash/panic/WDT keywords
  without sending commands, logging raw serial text, or overcounting split boundary matches.

Note:
  Screen capture holds the LVGL lock on the device during frame extraction;
  it is not intended for high-frequency polling or FPS measurements.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import errno
import fcntl
import hashlib
import json
import math
import os
import re
import select
import struct
import sys
import tempfile
import termios
import time
import tty
import zlib
from typing import Callable, Dict, Tuple


# Protocol constants
PROTOCOL_COMMAND = b"FAP_SCREENSHOT_V1\n"
PROTOCOL_HEADER_PREFIX = b"FAP_SCREENSHOT_V1 "
DEFAULT_BAUDRATE = 115200
DEFAULT_TIMEOUT = 5.0
MAX_HEADER_LINE_BYTES = 256
MAX_LEADING_NOISE_BUFFER = 512
MAX_ALLOWED_BYTES = 2 * 1024 * 1024  # 2 MiB safety ceiling
MAX_DURATION_SECONDS = 300.0         # 5 minutes upper bound

# Passive monitor stability keywords
STABILITY_PATTERNS = {
    "guru_meditation": re.compile(rb"Guru Meditation", re.IGNORECASE),
    "watchdog": re.compile(rb"(?:WDT|Watchdog)", re.IGNORECASE),
    "assertion_failure": re.compile(rb"(?:assert|assertion failed)", re.IGNORECASE),
    "reboot": re.compile(rb"(?:rst:0x|rebooting|restart)", re.IGNORECASE),
}


def baud_to_constant(baudrate: int) -> int:
    """Maps integer baudrate to termios constant."""
    name = f"B{baudrate}"
    if hasattr(termios, name):
        return getattr(termios, name)
    raise ValueError(f"Unsupported baudrate: {baudrate}")


def validate_duration(name: str, value: float, max_val: float = MAX_DURATION_SECONDS) -> float:
    """Validates finite positive durations within acceptable bounds."""
    if value is None or not isinstance(value, (int, float)):
        raise TypeError(f"{name} must be a number.")
    if math.isnan(value) or math.isinf(value) or value <= 0:
        raise ValueError(f"{name} must be a finite positive number, got {value}.")
    if value > max_val:
        raise ValueError(f"{name} ({value}s) exceeds maximum allowed limit of {max_val}s.")
    return float(value)


def open_serial_posix(port: str, baudrate: int = DEFAULT_BAUDRATE) -> Tuple[int, list]:
    """Opens a serial device safely without triggering DTR/RTS hardware reset.

    Targeted at ESP32-C3 native USB Serial/JTAG (CDC-ACM).
    Validates baudrate before open; guarantees clean descriptor cleanup on any setup failure.
    """
    # 1. Validate baud rate before touching filesystem or opening descriptor
    baud_const = baud_to_constant(baudrate)

    fd = -1
    orig_attrs = None
    exclusive_acquired = False

    try:
        fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)

        # 2. Acquire exclusive device lock
        if hasattr(termios, "TIOCEXCL"):
            try:
                fcntl.ioctl(fd, termios.TIOCEXCL)
                exclusive_acquired = True
            except OSError as err:
                if err.errno == getattr(errno, "EBUSY", 16):
                    raise OSError(err.errno, f"Serial port {port} is busy / locked by another process.") from err
                unsupported_errs = (
                    getattr(errno, "ENOTTY", 25),
                    getattr(errno, "EINVAL", 22),
                    getattr(errno, "EOPNOTSUPP", 45),
                    getattr(errno, "ENOSYS", 78),
                )
                if err.errno not in unsupported_errs:
                    raise

        orig_attrs = termios.tcgetattr(fd)

        # 3. Configure raw non-blocking serial mode
        tty.setraw(fd)
        attrs = termios.tcgetattr(fd)

        # Clear HUPCL on active configuration (avoids modem hangup during session)
        if hasattr(termios, "HUPCL"):
            attrs[2] &= ~termios.HUPCL

        # Disable software flow control and translations
        attrs[0] &= ~(termios.IXON | termios.IXOFF | termios.IXANY)
        # 8 data bits, enable receiver, ignore modem control lines
        attrs[2] &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB)
        attrs[2] |= (termios.CS8 | termios.CREAD | termios.CLOCAL)

        # Disable hardware flow control
        if hasattr(termios, "CRTSCTS"):
            attrs[2] &= ~termios.CRTSCTS

        # Set input/output baud rate
        attrs[4] = baud_const
        attrs[5] = baud_const

        # Non-blocking polling
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0

        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        return fd, orig_attrs

    except Exception:
        if fd >= 0:
            try:
                if exclusive_acquired and hasattr(termios, "TIOCNXCL"):
                    try:
                        fcntl.ioctl(fd, termios.TIOCNXCL)
                    except (OSError, termios.error):
                        pass
                if orig_attrs is not None:
                    try:
                        termios.tcsetattr(fd, termios.TCSANOW, orig_attrs)
                    except (OSError, termios.error):
                        pass
            finally:
                # Guaranteed closure even if termios/ioctl operations raise termios.error or OSError
                try:
                    os.close(fd)
                except OSError:
                    pass
        raise


def close_serial_posix(fd: int, orig_attrs: list | None) -> None:
    """Restores original termios attributes and releases serial file descriptor."""
    try:
        if orig_attrs is not None:
            try:
                termios.tcsetattr(fd, termios.TCSANOW, orig_attrs)
            except (OSError, termios.error):
                pass
        if hasattr(termios, "TIOCNXCL"):
            try:
                fcntl.ioctl(fd, termios.TIOCNXCL)
            except (OSError, termios.error):
                pass
    finally:
        os.close(fd)


def send_command_nonblocking(fd: int, command: bytes, timeout: float, deadline: float | None = None) -> None:
    """Transmits bytes across a non-blocking descriptor respecting deadline and EAGAIN."""
    timeout = validate_duration("timeout", timeout)
    if deadline is None:
        deadline = time.monotonic() + timeout

    sent = 0
    total = len(command)

    while sent < total:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("Timed out while transmitting screenshot command.")

        _, wlist, _ = select.select([], [fd], [], min(remaining, 0.2))
        if not wlist:
            continue

        try:
            n = os.write(fd, command[sent:])
            sent += n
        except (BlockingIOError, InterruptedError):
            continue


def parse_screenshot_stream(
    read_fn: Callable[[int, float], bytes],
    timeout: float = DEFAULT_TIMEOUT,
    max_bytes: int = MAX_ALLOWED_BYTES,
    deadline: float | None = None
) -> Tuple[int, int, str, bytes]:
    """Parses incoming stream using a bounded buffer, extracting header and exact payload.

    Security & Robustness Invariants:
    - Bounded noise buffer: discards leading boot logs while preserving split prefix tokens.
    - Strict 256-byte header limit: checked both before and upon newline arrival.
    - Sanitized exceptions: never echoes raw serial text or tokens into error messages.
    - Exact payload isolation: switches to exact byte-count reading once newline is consumed.
    """
    timeout = validate_duration("timeout", timeout)
    if deadline is None:
        deadline = time.monotonic() + timeout

    rx_buffer = bytearray()
    header_parsed = False
    prefix_found_pos = -1
    width = 0
    height = 0
    fmt = ""
    expected_bytes = 0
    payload = bytearray()

    prefix_len = len(PROTOCOL_HEADER_PREFIX)

    while True:
        now = time.monotonic()
        remaining = deadline - now
        if remaining <= 0:
            raise TimeoutError("Timed out waiting for complete screenshot response.")

        chunk = read_fn(4096, remaining)
        if not chunk:
            continue

        if not header_parsed:
            rx_buffer.extend(chunk)

            if prefix_found_pos == -1:
                prefix_pos = rx_buffer.find(PROTOCOL_HEADER_PREFIX)
                if prefix_pos != -1:
                    prefix_found_pos = prefix_pos
                elif len(rx_buffer) > MAX_LEADING_NOISE_BUFFER:
                    # Keep only tail bytes to prevent splitting prefix across chunks
                    rx_buffer = rx_buffer[-(prefix_len - 1):]
                    continue

            if prefix_found_pos != -1:
                newline_pos = rx_buffer.find(b"\n", prefix_found_pos)
                if newline_pos != -1:
                    header_len = newline_pos - prefix_found_pos
                    if header_len > MAX_HEADER_LINE_BYTES:
                        raise ValueError("Malformed protocol header: header line exceeded 256 bytes.")

                    header_bytes = bytes(rx_buffer[prefix_found_pos:newline_pos])
                    tokens = header_bytes.decode("ascii", errors="replace").strip().split()

                    if len(tokens) != 5:
                        raise ValueError(f"Malformed protocol header: expected 5 tokens, found {len(tokens)}.")
                    if tokens[0] != "FAP_SCREENSHOT_V1":
                        raise ValueError("Malformed protocol header: invalid prefix token.")

                    try:
                        width = int(tokens[1])
                        height = int(tokens[2])
                        fmt = tokens[3]
                        expected_bytes = int(tokens[4])
                    except ValueError as err:
                        raise ValueError("Malformed protocol header: non-integer dimension or byte count.") from err

                    if width <= 0 or height <= 0:
                        raise ValueError(f"Invalid screenshot dimensions ({width}x{height}); must be positive.")
                    if width > 1024 or height > 1024:
                        raise ValueError(f"Screenshot dimensions ({width}x{height}) exceed maximum safety limit (1024x1024).")
                    if fmt != "RGB565LE":
                        raise ValueError("Malformed protocol header: unsupported pixel format; expected 'RGB565LE'.")
                    if expected_bytes != width * height * 2:
                        raise ValueError("Header byte count does not match width * height * 2.")
                    if expected_bytes > max_bytes:
                        raise ValueError(f"Payload size ({expected_bytes} bytes) exceeds maximum limit ({max_bytes} bytes).")

                    header_parsed = True
                    # Everything after the newline is binary payload
                    payload.extend(rx_buffer[newline_pos + 1:])
                    rx_buffer.clear()
                elif (len(rx_buffer) - prefix_found_pos) > MAX_HEADER_LINE_BYTES:
                    raise ValueError("Malformed protocol header: header line exceeded 256 bytes without a newline.")
        else:
            payload.extend(chunk)

        if header_parsed and len(payload) >= expected_bytes:
            break

    exact_payload = bytes(payload[:expected_bytes])
    return width, height, fmt, exact_payload


def rgb565le_to_rgb888(raw_bytes: bytes, width: int, height: int) -> bytes:
    """Converts raw RGB565 little-endian pixels to RGB888 bytes."""
    expected_len = width * height * 2
    if len(raw_bytes) != expected_len:
        raise ValueError(f"Buffer size {len(raw_bytes)} does not match {width}x{height} RGB565 ({expected_len}).")

    out = bytearray(width * height * 3)
    src_idx = 0
    dst_idx = 0

    while src_idx < expected_len:
        val = raw_bytes[src_idx] | (raw_bytes[src_idx + 1] << 8)
        src_idx += 2

        r = ((val >> 11) & 0x1F) * 255 // 31
        g = ((val >> 5) & 0x3F) * 255 // 63
        b = (val & 0x1F) * 255 // 31

        out[dst_idx] = r
        out[dst_idx + 1] = g
        out[dst_idx + 2] = b
        dst_idx += 3

    return bytes(out)


def encode_png(rgb_bytes: bytes, width: int, height: int) -> bytes:
    """Encodes 24-bit RGB pixel bytes into a valid PNG image using standard library zlib."""
    signature = b"\x89PNG\r\n\x1a\n"

    ihdr_data = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    ihdr_crc = struct.pack(">I", zlib.crc32(b"IHDR" + ihdr_data) & 0xFFFFFFFF)
    ihdr_chunk = struct.pack(">I", len(ihdr_data)) + b"IHDR" + ihdr_data + ihdr_crc

    row_bytes = width * 3
    scanlines = bytearray()
    for y in range(height):
        scanlines.append(0)
        scanlines.extend(rgb_bytes[y * row_bytes:(y + 1) * row_bytes])

    compressed_idat = zlib.compress(scanlines, level=6)
    idat_crc = struct.pack(">I", zlib.crc32(b"IDAT" + compressed_idat) & 0xFFFFFFFF)
    idat_chunk = struct.pack(">I", len(compressed_idat)) + b"IDAT" + compressed_idat + idat_crc

    iend_crc = struct.pack(">I", zlib.crc32(b"IEND") & 0xFFFFFFFF)
    iend_chunk = struct.pack(">I", 0) + b"IEND" + iend_crc

    return signature + ihdr_chunk + idat_chunk + iend_chunk


def save_capture_outputs(
    output_prefix: str,
    width: int,
    height: int,
    fmt: str,
    raw_bytes: bytes,
    port: str,
    baudrate: int,
    overwrite: bool = False
) -> Dict[str, str]:
    """Writes .raw, .png, and sanitized .json files with SHA-256 integrity verification."""
    raw_path = f"{output_prefix}.raw"
    png_path = f"{output_prefix}.png"
    json_path = f"{output_prefix}.json"

    if not overwrite:
        for p in (raw_path, png_path, json_path):
            if os.path.exists(p):
                raise FileExistsError(f"Output file already exists: {p}; specify a different --output or pass overwrite=True.")

    parent_dir = os.path.dirname(os.path.abspath(output_prefix))
    if parent_dir:
        os.makedirs(parent_dir, exist_ok=True)

    with open(raw_path, "wb" if overwrite else "xb") as f:
        f.write(raw_bytes)

    rgb888 = rgb565le_to_rgb888(raw_bytes, width, height)
    png_bytes = encode_png(rgb888, width, height)
    with open(png_path, "wb" if overwrite else "xb") as f:
        f.write(png_bytes)

    sha256_hash = hashlib.sha256(raw_bytes).hexdigest()
    timestamp_utc = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")

    meta = {
        "timestamp_utc": timestamp_utc,
        "port": port,
        "baudrate": baudrate,
        "width": width,
        "height": height,
        "format": fmt,
        "bytes": len(raw_bytes),
        "raw_sha256": sha256_hash,
        "notes": (
            "Capture holds the LVGL lock on the device during readout; "
            "do not use for high-frequency polling or FPS measurements."
        ),
    }

    with open(json_path, "w" if overwrite else "x", encoding="utf-8") as f:
        json.dump(meta, f, indent=2)

    return {
        "raw": raw_path,
        "png": png_path,
        "json": json_path,
        "sha256": sha256_hash,
    }


def run_capture(
    port: str,
    output_prefix: str,
    baudrate: int = DEFAULT_BAUDRATE,
    timeout: float = DEFAULT_TIMEOUT,
    overwrite: bool = False
) -> Dict[str, str]:
    """Performs end-to-end device screenshot capture with a single shared total deadline."""
    timeout = validate_duration("timeout", timeout)
    shared_deadline = time.monotonic() + timeout

    fd, orig_attrs = open_serial_posix(port, baudrate)

    try:
        termios.tcflush(fd, termios.TCIFLUSH)

        # Transmit screenshot command sharing total deadline
        send_command_nonblocking(fd, PROTOCOL_COMMAND, timeout=timeout, deadline=shared_deadline)

        def reader(max_chunk: int, timeout_sec: float) -> bytes:
            rlist, _, _ = select.select([fd], [], [], timeout_sec)
            if rlist:
                return os.read(fd, max_chunk)
            return b""

        width, height, fmt, payload = parse_screenshot_stream(reader, timeout=timeout, deadline=shared_deadline)
        return save_capture_outputs(
            output_prefix=output_prefix,
            width=width,
            height=height,
            fmt=fmt,
            raw_bytes=payload,
            port=port,
            baudrate=baudrate,
            overwrite=overwrite
        )
    finally:
        close_serial_posix(fd, orig_attrs)


def run_listen(
    port: str,
    listen_seconds: float,
    output_prefix: str | None = None,
    baudrate: int = DEFAULT_BAUDRATE,
    overwrite: bool = False
) -> Dict[str, object]:
    """Passively listens to incoming serial stream without issuing commands."""
    listen_seconds = validate_duration("listen_seconds", listen_seconds)
    fd, orig_attrs = open_serial_posix(port, baudrate)

    start_time = time.monotonic()
    deadline = start_time + listen_seconds
    total_bytes = 0
    keyword_counts = {k: 0 for k in STABILITY_PATTERNS}

    max_pattern_len = 64
    prev_tail = b""

    try:
        while True:
            now = time.monotonic()
            remaining = deadline - now
            if remaining <= 0:
                break

            rlist, _, _ = select.select([fd], [], [], min(remaining, 0.2))
            if not rlist:
                continue

            chunk = os.read(fd, 4096)
            if not chunk:
                continue

            total_bytes += len(chunk)
            combined = prev_tail + chunk
            boundary_offset = len(prev_tail)

            # Match keywords only once (must end within the newly arrived chunk)
            for key, pattern in STABILITY_PATTERNS.items():
                for m in pattern.finditer(combined):
                    if m.end() > boundary_offset:
                        keyword_counts[key] += 1

            prev_tail = combined[-max_pattern_len:] if len(combined) > max_pattern_len else combined

        elapsed = time.monotonic() - start_time
        timestamp_utc = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")

        summary = {
            "mode": "listen",
            "timestamp_utc": timestamp_utc,
            "port": port,
            "baudrate": baudrate,
            "listen_seconds": listen_seconds,
            "elapsed_seconds": round(elapsed, 3),
            "bytes_received": total_bytes,
            "keyword_matches": keyword_counts,
            "notes": (
                "Passive serial monitoring only; absence of keywords does not establish "
                "overall device stability or hardware certification."
            ),
        }

        if output_prefix:
            json_path = f"{output_prefix}.json"
            if not overwrite and os.path.exists(json_path):
                raise FileExistsError(f"Output file already exists: {json_path}")
            parent_dir = os.path.dirname(os.path.abspath(output_prefix))
            if parent_dir:
                os.makedirs(parent_dir, exist_ok=True)
            with open(json_path, "w" if overwrite else "x", encoding="utf-8") as f:
                json.dump(summary, f, indent=2)

        return summary
    finally:
        close_serial_posix(fd, orig_attrs)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="POSIX device screenshot and passive serial listener for FoloToy AI Passport."
    )
    parser.add_argument(
        "--port",
        required=True,
        help="Serial port path (e.g., /dev/cu.usbmodem2101 or /dev/ttyACM0)."
    )
    parser.add_argument(
        "--output",
        default=None,
        help="Output path prefix for .raw, .png, and .json files (defaults to secure /tmp path)."
    )
    parser.add_argument(
        "--baudrate",
        type=int,
        default=DEFAULT_BAUDRATE,
        help=f"Baud rate (default: {DEFAULT_BAUDRATE})."
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT,
        help=f"Maximum wait timeout in seconds for screenshot capture (default: {DEFAULT_TIMEOUT})."
    )
    parser.add_argument(
        "--listen-seconds",
        type=float,
        default=None,
        help="Run in passive listen mode for N seconds without sending commands."
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="Overwrite existing output files if they exist."
    )

    args = parser.parse_args(argv)

    output_prefix = args.output
    if not output_prefix and args.listen_seconds is None:
        output_prefix = os.path.join(tempfile.mkdtemp(prefix="fap_capture_"), "screen")

    try:
        if args.listen_seconds is not None:
            summary = run_listen(
                port=args.port,
                listen_seconds=args.listen_seconds,
                output_prefix=args.output,
                baudrate=args.baudrate,
                overwrite=args.overwrite
            )
            print(json.dumps(summary, indent=2))
            return 0

        res = run_capture(
            port=args.port,
            output_prefix=output_prefix,
            baudrate=args.baudrate,
            timeout=args.timeout,
            overwrite=args.overwrite
        )
        print("Captured screen successfully:")
        print(f"  Raw RGB565: {res['raw']}")
        print(f"  PNG Image:  {res['png']}")
        print(f"  JSON Meta:  {res['json']}")
        print(f"  SHA-256:    {res['sha256']}")
        return 0

    except (ValueError, TypeError, TimeoutError, OSError) as err:
        print(f"Error: {err}", file=sys.stderr)
        return 1
    except Exception as err:
        print(f"Unexpected error: {type(err).__name__}: {err}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
