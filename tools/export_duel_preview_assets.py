#!/usr/bin/env python3

import re
import struct
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def main():
    source = (ROOT / 'main/duel_assets.c').read_text()
    destination = ROOT / 'assets/images/time-duel/device'
    destination.mkdir(parents=True, exist_ok=True)
    for name, width, height, has_alpha in [
            ('duel_outpost', 240, 240, False), ('duel_agent_0', 80, 112, True),
            ('duel_agent_1', 80, 112, True), ('duel_agent_0_win', 80, 112, True),
            ('duel_agent_1_win', 80, 112, True)]:
        body = re.search(rf'{name}_data\[\] = \{{(.*?)\}};', source, re.S).group(1)
        raw = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-f]{2})', body))
        color_size = width * height * 2
        assert len(raw) == width * height * (3 if has_alpha else 2)
        alpha = raw[color_size:] if has_alpha else None
        pixels = bytearray()
        for row in range(height):
            pixels.append(0)
            offset = row * width * 2
            for column, (color,) in enumerate(
                    struct.iter_unpack('<H', raw[offset:offset + width * 2])):
                red, green, blue = color >> 11, (color >> 5) & 63, color & 31
                pixels.extend(((red << 3) | (red >> 2), (green << 2) | (green >> 4),
                               (blue << 3) | (blue >> 2)))
                if has_alpha:
                    pixels.append(alpha[row * width + column])
        color_type = 6 if has_alpha else 2
        header = struct.pack('>IIBBBBB', width, height, 8, color_type, 0, 0, 0)
        png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header)
        png += chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b'')
        (destination / f'{name}.png').write_bytes(png)
        print(f'{name}: {width} x {height}, {len(png)} bytes')


if __name__ == '__main__':
    main()
