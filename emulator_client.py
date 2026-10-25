"""Exercise Pinball9588 on a fresh, isolated full-firmware NAND.

Uses only the emulator's public HTTP input/screen/file APIs. The supplied
emulator installation and its existing NANDs are never changed. Each run has
its own NAND, logs and cache in qa/runtime; screenshots/report stay in qa.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import socket
import struct
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile
import zlib

HERE = Path(__file__).resolve().parent
SDK = HERE / "sdk"


def png_rgb(data: bytes) -> tuple[int, int, bytes]:
    """Decode the RGB PNGs emitted by the emulator, without Pillow."""
    assert data.startswith(b"\x89PNG\r\n\x1a\n"), "Screenshot is not PNG"
    offset, packed = 8, bytearray()
    width = height = 0
    while offset < len(data):
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        chunk = data[offset + 8:offset + 8 + length]
        if kind == b"IHDR":
            width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", chunk)
            assert (depth, color, compression, filtering, interlace) == (8, 2, 0, 0, 0)
        elif kind == b"IDAT":
            packed.extend(chunk)
        offset += length + 12
    raw = zlib.decompress(packed)
    stride = width * 3
    assert len(raw) == height * (stride + 1)
    # The emulator's own PNG writer always uses filter zero.
    assert all(raw[y * (stride + 1)] == 0 for y in range(height)), "Unexpected PNG filter"
    return width, height, b"".join(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)] for y in range(height))


def pixel_changes(a: bytes, b: bytes, box: tuple[int, int, int, int] | None = None) -> int:
    aw, ah, ar = png_rgb(a)
    bw, bh, br = png_rgb(b)
    assert (aw, ah) == (bw, bh) == (240, 320), ((aw, ah), (bw, bh))
    x0, y0, x1, y1 = box or (0, 0, aw, ah)
    return sum(ar[(y * aw + x) * 3:(y * aw + x + 1) * 3] != br[(y * aw + x) * 3:(y * aw + x + 1) * 3]
               for y in range(y0, y1) for x in range(x0, x1))


class Frontend:
    def __init__(self, port: int, out: Path):
        self.base = f"http://127.0.0.1:{port}"
        self.out = out
        self.opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

    def request(self, endpoint: str, post: bool = False, timeout: float = 30, **params):
        url = self.base + endpoint
        if params:
            url += "?" + urllib.parse.urlencode(params)
        req = urllib.request.Request(url, data=b"" if post else None)
        with self.opener.open(req, timeout=timeout) as response:
            data = response.read()
            if "json" in response.headers.get("content-type", ""):
                obj = json.loads(data)
                assert not obj.get("error"), obj
                return obj
            return data

    def key(self, code: int, seconds: float = .20):
        self.request("/api/key", True, code=code, down=1)
        time.sleep(seconds)
        self.request("/api/key", True, code=code, down=0)
        time.sleep(.15)

    def touch(self, x: int, y: int, seconds: float = .20):
        self.request("/api/touch", True, x=x, y=y, down=1)
        time.sleep(seconds)
        self.request("/api/touch", True, x=x, y=y, down=0)
        time.sleep(.15)

    def shot(self, name: str):
        data = self.request("/screen.png")
        png_rgb(data)
        (self.out / f"{name}.png").write_bytes(data)
        return data

    def shot_changed(self, name: str, before: bytes, box, minimum: int, timeout: float = 2):
        """Keep input held until a frame demonstrates its visible response.

        QEMU's adaptive instruction clock and /screen.png cache can delay a
        newly applied key beyond a fixed 250ms wall-clock sleep at boot.
        """
        deadline = time.monotonic() + timeout
        while True:
            data = self.shot(name)
            if pixel_changes(before, data, box) > minimum:
                return data
            if time.monotonic() >= deadline:
                return data  # The unchanged assertion below still fails.
            time.sleep(.10)
