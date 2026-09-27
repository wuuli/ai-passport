#!/usr/bin/env python3
"""Serve the two game previews and their public assets on loopback."""
import argparse
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit
ROOT=Path(__file__).resolve().parent.parent
class Handler(SimpleHTTPRequestHandler):
    def translate_path(self, path):
        path=unquote(urlsplit(path).path)
        if path in ('/','/index.html','/exit-corridor.html'):
            return str(ROOT/'prototype/exit-corridor.html')
        if path in ('/games.html', '/time-duel-v2.html', '/time-duel-engine.js',
                    '/time-duel-preview.js', '/time-duel-audio.js',
                    '/time-duel-military.css'):
            return str(ROOT/'prototype'/path[1:])
        prefixes={'/exit-corridor/':ROOT/'prototype/exit-corridor',
                  '/time-duel/':ROOT/'prototype/time-duel',
                  '/assets/images/time-duel/':ROOT/'assets/images/time-duel',
                  '/assets/images/exit-corridor/':ROOT/'assets/images/exit-corridor'}
        for prefix,base in prefixes.items():
            if path.startswith(prefix):
                result=(base/path[len(prefix):]).resolve()
                if result.is_relative_to(base):return str(result)
        return str(ROOT/'__not_a_served_path__')
    def list_directory(self,path):
        self.send_error(404);return None
    def end_headers(self):
        self.send_header('Cache-Control','no-cache')
        super().end_headers()
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--port',type=int,default=8098);args=parser.parse_args()
    print(f'Corridor firmware preview: http://127.0.0.1:{args.port}/',flush=True)
    ThreadingHTTPServer(('127.0.0.1',args.port),Handler).serve_forever()
