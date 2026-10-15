#!/usr/bin/env python3
"""Formula T GUI server (given code, not part of the assignment).

Run from anywhere inside the container:

    python3 gui/serve.py [port]        (default 8000; VS Code forwards it)

then open http://localhost:8000. It serves the page (gui/) and the programs'
outputs (results/), never cached so the page always sees the latest file,
and turns the page's red flag button (POST /red-flag) into the file
results/red-flag.request, which build/race picks up.
"""

import http.server
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8000


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def do_GET(self):
        if self.path == "/":
            self.send_response(302)
            self.send_header("Location", "/gui/")
            self.end_headers()
            return
        super().do_GET()

    def do_POST(self):
        if self.path != "/red-flag":
            self.send_error(404)
            return
        results = ROOT / "results"
        results.mkdir(exist_ok=True)
        (results / "red-flag.request").touch()
        print("bandera roja pedida")
        self.send_response(204)
        self.end_headers()

    def log_message(self, format, *args):
        pass  # the page polls every 100 ms: logging each GET would flood the terminal


if __name__ == "__main__":
    print(f"GUI de Formula T: http://localhost:{PORT}  (Ctrl-C para cortar)")
    http.server.ThreadingHTTPServer(("", PORT), Handler).serve_forever()
