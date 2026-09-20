#!/usr/bin/env python3
"""Compile the firmware game/renderer unchanged into a browser reactor module."""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'prototype/exit-corridor/firmware'
SOURCES = ['main/corridor_game.c', 'main/corridor_game.h', 'main/corridor_render.c',
           'main/corridor_render.h', 'main/corridor_palette.inc', 'main/corridor_sprite_lut.inc',
           'main/corridor_notice.inc', 'main/corridor_font.c',
           'main/demo_corridor.c', 'prototype/exit-corridor/firmware/bridge.c',
           'assets/images/exit-corridor/commuter-device.bin', 'tools/build_corridor_web.py']
EXPORTS = ['web_sprite', 'web_sprite_size', 'web_init', 'web_key', 'web_tick', 'web_pause',
           'web_title', 'web_draw', 'web_destroy', 'web_state', 'web_review']
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def font_data():
    source = (ROOT/'main/corridor_font.c').read_text()
    bitmap = re.search(r'glyph_bitmap\[\]\s*=\s*\{(.*?)\};', source, re.S)[1]
    descriptors = re.search(r'glyph_dsc\[\]\s*=\s*\{(.*?)\};', source, re.S)[1]
    glyphs = [{k:int(v) for k,v in re.findall(r'\.(\w+)\s*=\s*(-?\d+)', item)}
              for item in re.findall(r'\{([^}]+)\}', descriptors)]
    sparse = re.search(r'unicode_list_1\[\]\s*=\s*\{(.*?)\};', source, re.S)[1]
    mapping = {str(i):i-31 for i in range(32,127)}
    mapping.update({str(12290+int(n,16)):96+i for i,n in enumerate(re.findall(r'0x[0-9a-f]+',sparse))})
    assert '.bpp = 2' in source and '.bitmap_format = 0' in source
    return dict(bitmap=[int(n,16) for n in re.findall(r'0x[0-9a-f]+',bitmap)],glyphs=glyphs,
                mapping=mapping,lineHeight=int(re.search(r'\.line_height = (\d+)',source)[1]),
                baseline=int(re.search(r'\.base_line = (\d+)',source)[1]))
def ui_data():
    # Fail generation if firmware copy changes, instead of silently keeping stale UI.
    tokens = [json.loads(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"',(ROOT/'main/demo_corridor.c').read_text())]
    wanted={'title':'地下通道','goal':'找到 8 号出口。\n\n按 OK 进入','win':'8 号出口',
            'cleared':'你已走出通道。\n\n按 OK 再走一次','exit':'长按 OK 返回',
            'turning':'转身中','walking':'行走中','stopped':'OK 行走','status':'出口 %u  %s'}
    for s in wanted.values(): assert s in tokens, f'Firmware UI changed: {s}'
    return wanted

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',default=os.environ.get('WASI_SDK_PATH'))
    parser.add_argument('--check',action='store_true',help='Verify shipped artifact/source hashes without a compiler')
    args=parser.parse_args()
    manifest_path=OUT/'manifest.json'
    if args.check:
        m=json.loads(manifest_path.read_text())
        for p,h in m['sources'].items(): assert digest(ROOT/p)==h, f'Stale browser build: {p}'
        for p,h in m['artifacts'].items(): assert digest(OUT/p)==h, f'Changed browser artifact: {p}'
        print('Firmware browser source/artifact parity: PASS');return
    if not args.sdk:parser.error('Set WASI_SDK_PATH or --sdk to wasi-sdk (tested: 34.0)')
    sdk=Path(args.sdk);clang=sdk/'bin/clang';sprite=ROOT/'assets/images/exit-corridor/commuter-device.bin'
    cmd=[str(clang),'-O2','-ffp-contract=off','-mexec-model=reactor','-Imain',
         '-D_POSIX_C_SOURCE=200809L',f'-DEC_SPRITE_BYTES={sprite.stat().st_size}',
         'main/corridor_game.c','main/corridor_render.c',str(OUT/'bridge.c'),'-lm',
         '-Wl,-z,stack-size=131072','-Wl,--initial-memory=2097152','-Wl,--max-memory=4194304',
         *['-Wl,--export='+e for e in EXPORTS],'-o',str(OUT/'corridor.wasm')]
    subprocess.run(cmd,cwd=ROOT,check=True)
    (OUT/'presentation.json').write_text(json.dumps({'font':font_data(),'ui':ui_data()},ensure_ascii=False,separators=(',',':'))+'\n')
    m={'schema':1,'compiler':subprocess.check_output([str(clang),'--version'],text=True).splitlines()[0],
       'nativeResolution':[240,320],'sources':{p:digest(ROOT/p) for p in SOURCES},
       'artifacts':{p:digest(OUT/p) for p in ['corridor.wasm','presentation.json']},
       'spriteBytes':sprite.stat().st_size,'spriteUrl':'/assets/images/exit-corridor/commuter-device.bin',
       'boundary':'Shared C game and renderer; browser input/display shell. Browser timing is not device timing.'}
    manifest_path.write_text(json.dumps(m,indent=2)+'\n')
    print('Built shared firmware browser:',(OUT/'corridor.wasm').stat().st_size,'bytes')
if __name__=='__main__':main()
