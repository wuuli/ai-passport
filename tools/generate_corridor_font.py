#!/usr/bin/env python3
"""Subset the project-standard OFL Noto Sans SC using lv_font_conv 1.5.3."""
import argparse
import re
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--font', required=True)
parser.add_argument('--converter', required=True)
parser.add_argument('--title', action='store_true', help='Generate the four-glyph 30px title subset')
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
symbols = ''.join(sorted(set(re.findall(r'[^\x00-\x7f]', (root/'main/demo_corridor.c').read_text()))))
output = root/('main/corridor_title_font.c' if args.title else 'main/corridor_font.c')
subprocess.run([args.converter, '--size', '30' if args.title else '16', '--bpp', '2', '--format', 'lvgl',
                '--no-compress', '--no-kerning', '--lv-include', 'lvgl.h',
                '--font', args.font, '--symbols', '8号出口' if args.title else symbols,
                *([] if args.title else ['-r', '32-126']), '-o', str(output)], check=True)
source = re.sub(r'/\*.*?\*/', '', output.read_text(), flags=re.S)
output.write_text(re.sub(r'\n{3,}', '\n\n', '\n'.join(line.rstrip() for line in source.splitlines())).rstrip('\n')+'\n')
