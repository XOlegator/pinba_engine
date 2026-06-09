---
title: "Pinba UDP Protocol"
type: concept
sources:
  - raw/docs/pinba-udp-protocol-proto.md
  - raw/repos/tony2001-pinba-engine-changelog.md
related:
  - wiki/concepts/pinba-data-flow.md
  - wiki/concepts/pinba-engine-internals.md
  - wiki/concepts/protobuf-runtime-strategy.md
confidence: high
updated: 2026-06-09
---

# Pinba UDP Protocol

## Transport

- UDP only. No TCP, no HTTP, no response.
- Default port: **30002**
- Encoding: Protocol Buffers v2 with `LITE_RUNTIME` (no reflection overhead)

## Message Structure

One `Pinba.Request` per UDP packet. Optional nested `repeated Request requests` field (18)
allows batching multiple requests in one packet (added in v1.1.0).

## Timer Encoding — Most Non-Obvious Part

Timers are stored as parallel arrays. To decode:
```
for each timer i:
  hit_count = timer_hit_count[i]
  value     = timer_value[i]
  tag_count = timer_tag_count[i]
  # consume next tag_count elements from timer_tag_name/timer_tag_value
  tags = [(dictionary[timer_tag_name[j]], dictionary[timer_tag_value[j]])
          for j in range(current_offset, current_offset + tag_count)]
```

The `dictionary` string array deduplicates repeated tag names/values (e.g., "group", "db", etc.)
across all timers in the request.

## Request-Level Tags vs Timer Tags

| Type | Fields | Purpose |
|------|--------|---------|
| Timer tags | `timer_tag_name/value` + `dictionary` | Identify what a timer measured (group, server, operation) |
| Request tags | `tag_name/value` + `dictionary` | Filter entire requests (api_version, user_type, etc.) |

Both use the same `dictionary` for deduplication.

## Field Versions and Defaults

Missing optional fields default to zero/empty when engine processes packet.
The engine handles both old (pre-1.1) and new packets gracefully.

## Protocol Stability

The `.proto` definition has been stable since 1.1.0 (2015). No breaking changes since then.
Both the server (this engine) and the client (PHP extension) ship the same `pinba.proto`
(fields 1–23). They deliberately use **different protobuf runtimes**: the engine decodes
with the C++ Protocol Buffers runtime and adapts the message into flat C-style structs via
a shim (`src/pinba.pb-c.cc`), while the PHP extension uses the protobuf-c (C) runtime. The
shared, stable artifact is the schema, not the generated code. See
[[protobuf-runtime-strategy]].

Note: fields `timer_ru_utime` (22) and `timer_ru_stime` (23) carry per-timer CPU usage as
`repeated float` arrays parallel to `timer_value`; they are decoded the same way as the
other parallel timer arrays.

See: [[pinba-data-flow]], [[pinba-engine-internals]], [[protobuf-runtime-strategy]]
