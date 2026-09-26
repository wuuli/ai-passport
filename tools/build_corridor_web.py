#!/usr/bin/env python3
"""Compile the firmware game/renderer unchanged into a browser reactor module."""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'prototype/exit-corridor/firmware'
SOURCES = ['main/corridor_game.c', 'main/corridor_game.h', 'main/corridor_render.c',
           'main/corridor_render.h', 'main/corridor_palette.inc', 'main/corridor_sprite_lut.inc',
           'main/corridor_notice.inc', 'main/corridor_font.c', 'main/corridor_title_font.c',
           'main/demo_corridor.c', 'prototype/exit-corridor/firmware/bridge.c',
           'assets/images/exit-corridor/commuter-device.bin', 'tools/build_corridor_web.py']
EXPORTS = ['web_sprite', 'web_sprite_size', 'web_init', 'web_key', 'web_tick', 'web_pause',
           'web_title', 'web_draw', 'web_destroy', 'web_state', 'web_review']
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def font_data(path='main/corridor_font.c'):
    source = (ROOT/path).read_text()
    bitmap = re.search(r'glyph_bitmap\[\]\s*=\s*\{(.*?)\};', source, re.S)[1]
    descriptors = re.search(r'glyph_dsc\[\]\s*=\s*\{(.*?)\};', source, re.S)[1]
    glyphs = [{k:int(v) for k,v in re.findall(r'\.(\w+)\s*=\s*(-?\d+)', item)}
              for item in re.findall(r'\{([^}]+)\}', descriptors)]
    cmaps = re.search(r'cmaps\[\]\s*=\s*\{(.*?)\};', source, re.S)[1]
    mapping = {}
    for item in re.findall(r'\{([^}]+)\}', cmaps):
        fields = dict(re.findall(r'\.(\w+)\s*=\s*(\w+)', item))
        start, first = int(fields['range_start']), int(fields['glyph_id_start'])
        if fields['type'] == 'LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY':
            offsets = range(int(fields['range_length']))
        else:
            assert fields['type'] == 'LV_FONT_FMT_TXT_CMAP_SPARSE_TINY'
            sparse = re.search(fields['unicode_list']+r'\[\]\s*=\s*\{(.*?)\};', source, re.S)[1]
            offsets = [int(n,16) for n in re.findall(r'0x[0-9a-f]+',sparse)]
        mapping.update({str(start+n):first+i for i,n in enumerate(offsets)})
    assert '.bpp = 2' in source and '.bitmap_format = 0' in source
    return dict(bitmap=[int(n,16) for n in re.findall(r'0x[0-9a-f]+',bitmap)],glyphs=glyphs,
                mapping=mapping,lineHeight=int(re.search(r'\.line_height = (\d+)',source)[1]),
                baseline=int(re.search(r'\.base_line = (\d+)',source)[1]))
def ui_data():
    # Fail generation if firmware copy changes, instead of silently keeping stale UI.
    tokens = [json.loads(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"',(ROOT/'main/demo_corridor.c').read_text())]
    wanted={'title':'8号出口','subtitle':'地下通道','start':'按 OK 进入','win':'8号出口',
            'cleared':'你已走出通道','replay':'按 OK 再走一次','exit':'长按 OK 返回',
            'guide':'顶部键：左转\n中部键：右转\n底部 OK 键：行走／停步\n拐角自动转向，停后按 OK',
            'turning':'转身中','walking':'行走中','stopped':'OK 行走','status':'出口 %u  %s'}
    for s in wanted.values(): assert s in tokens, f'Firmware UI changed: {s}'
    return wanted

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',default=os.environ.get('WASI_SDK_PATH'))
    parser.add_argument('--check',action='store_true',help='Verify shipped artifact/source hashes without a compiler')
    parser.add_argument('--presentation-only',action='store_true',help='Regenerate presentation/manifest metadata without compiling Wasm')
    args=parser.parse_args()
    manifest_path=OUT/'manifest.json'
    if args.check:
        m=json.loads(manifest_path.read_text())
        for p,h in m['sources'].items(): assert digest(ROOT/p)==h, f'Stale browser build: {p}'
        for p,h in m['artifacts'].items(): assert digest(OUT/p)==h, f'Changed browser artifact: {p}'
        print('Firmware browser source/artifact parity: PASS');return
    if not args.presentation_only and not args.sdk:parser.error('Set WASI_SDK_PATH or --sdk to wasi-sdk (tested: 34.0)')
    presentation_path=OUT/'presentation.json'
    presentation_path.write_text(json.dumps({'font':font_data(),'titleFont':font_data('main/corridor_title_font.c'),'ui':ui_data()},ensure_ascii=False,separators=(',',':'))+'\n')
    if args.presentation_only:
        m=json.loads(manifest_path.read_text())
        m['sources']={p:digest(ROOT/p) for p in SOURCES}
        m['artifacts']['presentation.json']=digest(presentation_path)
        m['spriteBytes']=(ROOT/'assets/images/exit-corridor/commuter-device.bin').stat().st_size
        manifest_path.write_text(json.dumps(m,indent=2)+'\n')
        print('Updated browser presentation metadata')
        return
    sdk=Path(args.sdk);clang=sdk/'bin/clang';sprite=ROOT/'assets/images/exit-corridor/commuter-device.bin'
    cmd=[str(clang),'-O2','-ffp-contract=off','-mexec-model=reactor','-Imain',
         '-D_POSIX_C_SOURCE=200809L',f'-DEC_SPRITE_BYTES={sprite.stat().st_size}',
         'main/corridor_game.c','main/corridor_render.c',str(OUT/'bridge.c'),'-lm',
         '-Wl,-z,stack-size=131072','-Wl,--initial-memory=2097152','-Wl,--max-memory=4194304',
         *['-Wl,--export='+e for e in EXPORTS],'-o',str(OUT/'corridor.wasm')]
    subprocess.run(cmd,cwd=ROOT,check=True)
    m={'schema':1,'compiler':subprocess.check_output([str(clang),'--version'],text=True).splitlines()[0],
       'nativeResolution':[240,320],'sources':{p:digest(ROOT/p) for p in SOURCES},
       'artifacts':{p:digest(OUT/p) for p in ['corridor.wasm','presentation.json']},
       'spriteBytes':sprite.stat().st_size,'spriteUrl':'/assets/images/exit-corridor/commuter-device.bin',
       'boundary':'Shared C game and renderer; browser input/display shell. Browser timing is not device timing.'}
    manifest_path.write_text(json.dumps(m,indent=2)+'\n')
    print('Built shared firmware browser:',(OUT/'corridor.wasm').stat().st_size,'bytes')
if __name__=='__main__':main()
