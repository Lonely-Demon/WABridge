"""Reference WABridge Protocol 1 envelope codec.

This module is intentionally dependency-free and is used as a protocol test
oracle. Native Windows and Android implementations must match these wire
invariants but must not copy Python into production.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass

MAGIC = 0x5742
VERSION = 1
HEADER = struct.Struct("!HBBHHQI")
HEADER_SIZE = HEADER.size

CHANNEL_LIMITS = {
    1: 64 * 1024,
    2: 4 * 1024 * 1024,
    3: 1 * 1024 * 1024,
    4: 1 * 1024 * 1024,
    5: 256 * 1024,
}


class ProtocolError(ValueError):
    pass


@dataclass(frozen=True)
class Envelope:
    channel: int
    kind: int
    flags: int
    request_id: int
    payload: bytes


def encode(message: Envelope) -> bytes:
    if message.channel not in CHANNEL_LIMITS:
        raise ProtocolError("unknown channel")
    if not 0 <= message.kind <= 0xFFFF:
        raise ProtocolError("kind out of range")
    if not 0 <= message.flags <= 0xFFFF:
        raise ProtocolError("flags out of range")
    if message.request_id == 0:
        raise ProtocolError("request id must be non-zero")
    if len(message.payload) == 0 or len(message.payload) > CHANNEL_LIMITS[message.channel]:
        raise ProtocolError("payload exceeds channel limit")
    return HEADER.pack(
        MAGIC,
        VERSION,
        message.channel,
        message.kind,
        message.flags,
        message.request_id,
        len(message.payload),
    ) + message.payload


def decode(frame: bytes) -> Envelope:
    if len(frame) < HEADER_SIZE:
        raise ProtocolError("truncated header")
    magic, version, channel, kind, flags, request_id, length = HEADER.unpack_from(frame)
    if magic != MAGIC:
        raise ProtocolError("bad magic")
    if version != VERSION:
        raise ProtocolError("unsupported version")
    if channel not in CHANNEL_LIMITS:
        raise ProtocolError("unknown channel")
    if request_id == 0:
        raise ProtocolError("zero request id")
    if length == 0 or length > CHANNEL_LIMITS[channel]:
        raise ProtocolError("invalid payload length")
    if len(frame) != HEADER_SIZE + length:
        raise ProtocolError("frame length mismatch")
    return Envelope(channel, kind, flags, request_id, frame[HEADER_SIZE:])
