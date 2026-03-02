from __future__ import annotations

import json
import os
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Dict

from .schema import ALLOWED_MACROS, parse_request_payload
from .service import interpret_request

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8787


class Handler(BaseHTTPRequestHandler):
    server_version = "MagicTrackSidecar/1.0"

    def _send_json(self, code: int, payload: Dict[str, object]) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self) -> None:  # noqa: N802
        if self.path.rstrip("/") != "/interpret":
            self._send_json(404, {"error": "not_found"})
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            raw = self.rfile.read(length).decode("utf-8", errors="replace")
            payload = json.loads(raw)
            if not isinstance(payload, dict):
                raise ValueError("payload must be object")
        except (ValueError, json.JSONDecodeError):
            self._send_json(400, {"error": "invalid_json"})
            return

        text, profile, allowed = parse_request_payload(payload)
        result = interpret_request(text, profile, allowed)
        self._send_json(200, result)

    def log_message(self, _format: str, *_args: object) -> None:
        return


def run_server() -> None:
    host = os.getenv("MAGICTRACK_SIDECAR_HOST", DEFAULT_HOST)
    port = int(os.getenv("MAGICTRACK_SIDECAR_PORT", str(DEFAULT_PORT)))
    server = ThreadingHTTPServer((host, port), Handler)
    print(f"MagicTrack sidecar listening on http://{host}:{port}/interpret")
    server.serve_forever()


__all__ = ["run_server", "Handler", "ALLOWED_MACROS"]
