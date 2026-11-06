"""Framing and parsing for the STM32 battery controller serial protocol.

Wire format: ``$<payload>*<4 hex CRC16-CCITT-FALSE>\n``.

The payload is normally a compact JSON object. The CRC covers only the payload
bytes between ``$`` and ``*``.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from typing import Any, Optional

_FRAME_RE = re.compile(rb"\$([^\r\n*]+)\*([0-9A-Fa-f]{4})\r?\n")
_HEX = "0123456789ABCDEF"


def crc16_ccitt(data: bytes, init: int = 0xFFFF) -> int:
    """CRC16-CCITT-FALSE with polynomial 0x1021."""
    crc = init
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc & 0xFFFF


def encode_frame(payload: str) -> bytes:
    """Return a complete ``$payload*CRC\\n`` frame."""
    raw = payload.encode("utf-8")
    crc = crc16_ccitt(raw)
    return b"$" + raw + b"*" + f"{crc:04X}".encode("ascii") + b"\n"


@dataclass(frozen=True)
class Frame:
    payload: str
    data: Optional[dict[str, Any]]
    crc_ok: bool
    error: Optional[str] = None


def parse_frame(line: bytes) -> Optional[Frame]:
    match = _FRAME_RE.fullmatch(line)
    if not match:
        return None
    raw_payload, raw_crc = match.groups()
    expected = int(raw_crc, 16)
    actual = crc16_ccitt(raw_payload)
    try:
        payload = raw_payload.decode("utf-8")
    except UnicodeDecodeError as exc:
        return Frame(payload="", data=None, crc_ok=False, error=f"utf8: {exc}")
    if expected != actual:
        return Frame(payload=payload, data=None, crc_ok=False, error="crc_mismatch")
    try:
        data = json.loads(payload)
    except json.JSONDecodeError:
        return Frame(payload=payload, data=None, crc_ok=True, error="not_json")
    if not isinstance(data, dict):
        return Frame(payload=payload, data=None, crc_ok=True, error="not_object")
    return Frame(payload=payload, data=data, crc_ok=True)


class FrameParser:
    """Incremental parser that tolerates arbitrary serial chunk boundaries."""

    def __init__(self, max_buffer: int = 8192) -> None:
        self._buffer = bytearray()
        self._max_buffer = max_buffer

    def feed(self, data: bytes) -> list[Frame]:
        frames: list[Frame] = []
        self._buffer.extend(data)
        while True:
            start = self._buffer.find(b"$")
            if start < 0:
                if len(self._buffer) > self._max_buffer:
                    self._buffer.clear()
                break
            if start > 0:
                del self._buffer[:start]
            end = self._buffer.find(b"\n")
            if end < 0:
                if len(self._buffer) > self._max_buffer:
                    del self._buffer[: len(self._buffer) - self._max_buffer]
                break
            line = bytes(self._buffer[: end + 1])
            del self._buffer[: end + 1]
            frame = parse_frame(line)
            if frame is not None:
                frames.append(frame)
        return frames

    def reset(self) -> None:
        self._buffer.clear()
