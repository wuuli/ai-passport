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
    for name, width, height in [('duel_outpost', 240, 240), ('duel_agent_0', 80, 112),
                                ('duel_agent_1', 80, 112), ('duel_agent_0_win', 80, 112),
                                ('duel_agent_1_win', 80, 112)]:
        body = re.search(rf'{name}_data\[\] = \{{(.*?)\}};', source, re.S).group(1)
        raw = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-f]{2})', body))
        assert len(raw) == width * height * 2
        pixels = bytearray()
        for offset in range(0, len(raw), width * 2):
            pixels.append(0)
            for (color,) in struct.iter_unpack('<H', raw[offset:offset + width * 2]):
                red, green, blue = color >> 11, (color >> 5) & 63, color & 31
                pixels.extend(((red << 3) | (red >> 2), (green << 2) | (green >> 4),
                               (blue << 3) | (blue >> 2)))
        header = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
        png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header)
        png += chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b'')
        (destination / f'{name}.png').write_bytes(png)
        print(f'{name}: {width} x {height}, {len(png)} bytes')


if __name__ == '__main__':
    main()
