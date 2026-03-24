#!/usr/bin/env python3
"""
Local dev server for the c2wasm demo.

Sets Cross-Origin-Opener-Policy and Cross-Origin-Embedder-Policy headers,
which are required for SharedArrayBuffer (used for interactive stdin via
Atomics.wait in a Web Worker).

Usage:  python3 server.py [port]
"""

import http.server
import sys


class COIHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        super().end_headers()

    def log_message(self, fmt, *args):
        super().log_message(fmt, *args)


if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    server = http.server.HTTPServer(('', port), COIHandler)
    print('Serving http://localhost:{} with cross-origin isolation headers'.format(port))
    print('Press Ctrl-C to stop.')
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print('\nStopped.')
