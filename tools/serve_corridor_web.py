#!/usr/bin/env python3
"""Serve only the corridor preview and its public assets on loopback."""
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
        prefixes={'/exit-corridor/':ROOT/'prototype/exit-corridor',
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
