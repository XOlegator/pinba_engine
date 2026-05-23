---
title: "Pinba UDP Protocol — Protobuf Spec"
type: source
sources:
  - raw/docs/pinba-udp-protocol-proto.md
related:
  - wiki/concepts/pinba-udp-protocol.md
  - wiki/concepts/pinba-data-flow.md
confidence: high
updated: 2026-05-23
---

# Pinba UDP Protocol — Protobuf Spec

## Key Facts

- **Encoding**: Protocol Buffers v2, `LITE_RUNTIME` (no reflection overhead)
- **Transport**: UDP, default port 30002
- **Direction**: PHP → Pinba Engine (one-way, no response)
- **Message**: single `Pinba.Request` per UDP packet (or nested for batching)

## Field Summary

Required (always present): hostname, server_name, script_name, request_count, document_size,
memory_peak, request_time, ru_utime, ru_stime (fields 1-9).

Optional/repeated (added progressively): status, memory_footprint, schema, request tags,
nested requests, timer rusage.

## Timer Encoding: Parallel Arrays + Dictionary

This is the most non-obvious part. Timers are encoded as parallel arrays:
- `timer_hit_count[i]`, `timer_value[i]` — metrics for timer i
- `timer_tag_count[i]` — how many tags timer i has
- `timer_tag_name[]`, `timer_tag_value[]` — flat list of tag indices (decode using timer_tag_count to split)
- `dictionary[]` — all unique strings; name/value are indices into this

This design deduplicates repeated tag strings across many timers.

## Field Version History (brief)

- Fields 1-9: original (v0.0.3)
- status (16): v0.0.4
- schema (19), memory_footprint (17), request tags (20,21), nested requests (18), timer rusage (22,23): v1.1.0

See [[pinba-udp-protocol]] for full decoded example.
