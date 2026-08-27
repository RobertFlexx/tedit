#!/usr/bin/env python3
import json
import sys


def read_message():
    headers = {}
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        if line == b"\r\n":
            break
        name, value = line.decode("ascii").split(":", 1)
        headers[name.lower()] = value.strip()
    return json.loads(sys.stdin.buffer.read(int(headers["content-length"])))


def send_message(message):
    body = json.dumps(message, separators=(",", ":")).encode()
    sys.stdout.buffer.write(f"Content-Length: {len(body)}\r\n\r\n".encode() + body)
    sys.stdout.buffer.flush()


while True:
    request = read_message()
    if request is None:
        break
    method = request.get("method")
    if method == "initialize":
        send_message({"jsonrpc": "2.0", "id": request["id"], "result": {"capabilities": {"documentFormattingProvider": True}}})
    elif method == "textDocument/formatting":
        send_message({
            "jsonrpc": "2.0",
            "id": request["id"],
            "result": [{
                "range": {"start": {"line": 0, "character": 0}, "end": {"line": 0, "character": 0}},
                "newText": "    ",
            }],
        })
