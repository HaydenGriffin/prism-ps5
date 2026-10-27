#!/usr/bin/env python3
# Prism for PS5 - Snapshot a real Home Assistant for the promotional renders.
# Copyright (C) 2026 Hayden Griffin
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes build/promo-data/data.json and its LG app icons from a live Home Assistant.

The file has the shape of tests/fixture.json: the Glasshouse TV's registry entries and
states, a config and the icons, so `make promo` renders the real screens with real apps.
It stays in the ignored build folder: it holds the TV's icon server address. No token is
written; the config gets a documentation address instead of the real one.

    python3 tools/capture-promo.py --host 192.0.2.20 --token-file ~/ha-token \\
        --favourite "BBC iPlayer" --favourite Afterglow --recent ITVX --remote-app "BBC iPlayer"
    make promo

Standard library only.
"""
import argparse
import base64
import json
import os
import socket
import struct
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "promo-data"


class Socket:
    """Just enough of a websocket client for three Home Assistant requests."""

    def __init__(self, host, port):
        self.sock = socket.create_connection((host, port), timeout=20)
        key = base64.b64encode(os.urandom(16)).decode()
        self.sock.sendall(
            f"GET /api/websocket HTTP/1.1\r\nHost: {host}:{port}\r\nUpgrade: websocket\r\n"
            f"Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n".encode())
        self.buffer = b""
        while b"\r\n\r\n" not in self.buffer:
            self.buffer += self.sock.recv(4096)
        self.buffer = self.buffer.split(b"\r\n\r\n", 1)[1]

    def exact(self, count):
        while len(self.buffer) < count:
            block = self.sock.recv(65536)
            if not block:
                raise SystemExit("Home Assistant closed the connection")
            self.buffer += block
        data, self.buffer = self.buffer[:count], self.buffer[count:]
        return data

    def receive(self):
        payload = b""
        while True:
            first, second = self.exact(2)
            length = second & 127
            if length == 126:
                length = struct.unpack(">H", self.exact(2))[0]
            elif length == 127:
                length = struct.unpack(">Q", self.exact(8))[0]
            payload += self.exact(length)
            if first & 0x80:
                return json.loads(payload)

    def send(self, value):
        data = json.dumps(value).encode()
        mask = os.urandom(4)
        if len(data) < 126:
            header = bytes([0x81, 0x80 | len(data)])
        else:
            header = bytes([0x81, 0x80 | 126]) + struct.pack(">H", len(data))
        self.sock.sendall(header + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))


def registries(host, port, token):
    ws = Socket(host, port)
    ws.receive()
    ws.send({"type": "auth", "access_token": token})
    if ws.receive().get("type") != "auth_ok":
        raise SystemExit("Home Assistant rejected the token")
    ws.send({"id": 1, "type": "config/entity_registry/list"})
    entities = ws.receive()["result"]
    ws.send({"id": 2, "type": "config/device_registry/list"})
    devices = ws.receive()["result"]
    return entities, devices


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--host", required=True, help="Home Assistant's numeric IPv4 address")
    parser.add_argument("--port", type=int, default=8123)
    parser.add_argument("--token-file", required=True, type=Path)
    parser.add_argument("--input", default="HDMI 1", help="the PS5's input option on the TV")
    parser.add_argument("--favourite", action="append", default=[], help="an LG app title, in order")
    parser.add_argument("--recent", action="append", default=[], help="an LG app title, newest first")
    parser.add_argument("--remote-app", help="the LG app title shown in the remote screen")
    args = parser.parse_args()
    token = args.token_file.expanduser().read_text().strip()

    entities, devices = registries(args.host, args.port, token)
    launchers = [e for e in entities if e["entity_id"].startswith("select.") and e.get("device_id")
                 and (e.get("unique_id") or "").endswith("_app") and not e.get("disabled_by")]
    if not launchers:
        raise SystemExit("no Glasshouse TV found")
    device = launchers[0]["device_id"]
    mine = [{key: e.get(key) for key in ("entity_id", "unique_id", "device_id", "disabled_by")}
            for e in entities if e.get("device_id") == device]
    named = [{key: d.get(key) for key in ("id", "name", "name_by_user")} for d in devices if d["id"] == device]

    request = urllib.request.Request(f"http://{args.host}:{args.port}/api/states",
                                     headers={"Authorization": f"Bearer {token}"})
    with urllib.request.urlopen(request, timeout=20) as response:
        live = {s["entity_id"]: s for s in json.load(response)}
    ids = {e["entity_id"] for e in mine}
    states = {i: {"s": live[i]["state"], "a": live[i]["attributes"]} for i in ids if i in live}

    launcher = states[launchers[0]["entity_id"]]["a"]
    titles = launcher.get("app_ids", {})
    unknown = [t for t in args.favourite + args.recent + [args.remote_app or ""] if t and t not in titles]
    if unknown:
        raise SystemExit(f"not LG app titles on this TV: {', '.join(unknown)}")

    # The launcher pictures show the PS5's own input, as they do while you browse Prism.
    input_app = "com.webos.app.hdmi" + args.input.split()[-1]
    active = next(e["entity_id"] for e in mine if (e.get("unique_id") or "").endswith("_active_app"))
    states[active]["s"] = args.input
    states[active]["a"].update({"app_id": input_app, "control_app_id": input_app, "keyboard_visible": False})

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "icons").mkdir(exist_ok=True)
    icons = {}
    for app, url in launcher.get("app_icons", {}).items():
        try:
            with urllib.request.urlopen(url, timeout=10) as response:
                (OUT / "icons" / f"{app}.png").write_bytes(response.read())
            icons[url] = f"icons/{app}.png"
        except OSError as error:
            print(f"skipped the icon for {app}: {error}")

    data = {
        "devices": named,
        "entities": mine,
        "states": {"a": states},
        "config": {"host": "192.0.2.20", "token": "promo", "device": device, "input": args.input,
                   "input_app": input_app, "favourites": [titles[t] for t in args.favourite],
                   "recent": [titles[t] for t in args.recent]},
        "icons": icons,
    }
    if args.remote_app:
        data["remote_app"] = titles[args.remote_app]
    (OUT / "data.json").write_text(json.dumps(data, indent=1))
    print(f"wrote {OUT / 'data.json'} with {len(icons)} icons")


if __name__ == "__main__":
    main()
