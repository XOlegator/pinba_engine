#!/usr/bin/env python3
# Copyright (c) 2025 Oleg Ekhlakov <subspam@mail.ru>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
"""Send Pinba UDP packets to a running Pinba Engine collector.

Encodes the ``Pinba::Request`` message (pinba.proto) directly in protobuf wire
format — no protobuf runtime, protoc, or pip install required — so the CI job
that exercises the engine's UDP ingest / aggregation paths stays self-contained
and version-independent.

Each packet carries the required request fields plus a timer with two tags and a
request-level tag (all resolved through the per-packet dictionary), so sending a
batch drives request processing, timer merging, tag creation, and report updates
in main.cc / data.cc / tags.cc.
"""

import argparse
import socket
import struct
import sys

# ── protobuf wire-format primitives (proto2, unpacked repeated fields) ──────────


def _varint(value: int) -> bytes:
    out = bytearray()
    v = value & 0xFFFFFFFFFFFFFFFF
    while True:
        b = v & 0x7F
        v >>= 7
        out.append(b | (0x80 if v else 0))
        if not v:
            return bytes(out)


def _tag(field: int, wire: int) -> bytes:
    return _varint((field << 3) | wire)


def _uint32(field: int, value: int) -> bytes:  # wire type 0
    return _tag(field, 0) + _varint(value)


def _float(field: int, value: float) -> bytes:  # wire type 5 (32-bit)
    return _tag(field, 5) + struct.pack("<f", value)


def _string(field: int, value: str) -> bytes:  # wire type 2
    raw = value.encode("utf-8")
    return _tag(field, 2) + _varint(len(raw)) + raw


def _repeated_uint32(field: int, values) -> bytes:
    return b"".join(_uint32(field, v) for v in values)


def _repeated_float(field: int, values) -> bytes:
    return b"".join(_float(field, v) for v in values)


def _repeated_string(field: int, values) -> bytes:
    return b"".join(_string(field, v) for v in values)


# Field numbers from pinba.proto's Request message.
F_HOSTNAME, F_SERVER_NAME, F_SCRIPT_NAME = 1, 2, 3
F_REQUEST_COUNT, F_DOCUMENT_SIZE, F_MEMORY_PEAK = 4, 5, 6
F_REQUEST_TIME, F_RU_UTIME, F_RU_STIME = 7, 8, 9
F_TIMER_HIT_COUNT, F_TIMER_VALUE = 10, 11
F_TIMER_TAG_COUNT, F_TIMER_TAG_NAME, F_TIMER_TAG_VALUE = 12, 13, 14
F_DICTIONARY, F_STATUS, F_SCHEMA = 15, 16, 19
F_TAG_NAME, F_TAG_VALUE = 20, 21


def build_request(i: int) -> bytes:
    """Build one varied Pinba request as a protobuf-encoded byte string."""
    script = f"/app/handler_{i % 4}.php"
    server = f"srv{i % 3}.example.com"
    host = f"web{i % 2:02d}"
    schema = "https" if i % 2 else "http"
    status = 200 if i % 5 else 404
    group_value = "A" if i % 2 else "B"

    # Per-packet dictionary; timer/request tags reference entries by index.
    #   0: "group"  1: <group_value>  2: "op"  3: "read"
    dictionary = ["group", group_value, "op", "read"]

    parts = [
        _string(F_HOSTNAME, host),
        _string(F_SERVER_NAME, server),
        _string(F_SCRIPT_NAME, script),
        _uint32(F_REQUEST_COUNT, 1),
        _uint32(F_DOCUMENT_SIZE, 1024 + i),
        _uint32(F_MEMORY_PEAK, 524288 + i),
        _float(F_REQUEST_TIME, 0.010 + (i % 100) * 0.001),
        _float(F_RU_UTIME, 0.004),
        _float(F_RU_STIME, 0.002),
        # One timer with two tags: group=<value>, op=read.
        _repeated_uint32(F_TIMER_HIT_COUNT, [1]),
        _repeated_float(F_TIMER_VALUE, [0.005 + (i % 50) * 0.0002]),
        _repeated_uint32(F_TIMER_TAG_COUNT, [2]),
        _repeated_uint32(F_TIMER_TAG_NAME, [0, 2]),
        _repeated_uint32(F_TIMER_TAG_VALUE, [1, 3]),
        _repeated_string(F_DICTIONARY, dictionary),
        _uint32(F_STATUS, status),
        _string(F_SCHEMA, schema),
        # One request-level tag: group=<value>.
        _repeated_uint32(F_TAG_NAME, [0]),
        _repeated_uint32(F_TAG_VALUE, [1]),
    ]
    return b"".join(parts)


def main() -> int:
    ap = argparse.ArgumentParser(description="Send Pinba UDP packets to a collector.")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=30002)
    ap.add_argument("--count", type=int, default=200)
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sent = 0
    try:
        for i in range(args.count):
            try:
                sock.sendto(build_request(i), (args.host, args.port))
                sent += 1
            except OSError as exc:
                # A prior packet may trigger an ICMP port-unreachable that surfaces as
                # ECONNREFUSED on a later send; tolerate transient per-packet errors.
                print(f"warning: send {i} failed: {exc}", file=sys.stderr)
    finally:
        sock.close()

    print(f"sent {sent}/{args.count} Pinba packets to {args.host}:{args.port}")
    return 0 if sent > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
