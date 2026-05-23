---
title: "Pinba UDP Protocol"
type: concept
sources:
  - raw/docs/pinba-udp-protocol-proto.md
  - raw/repos/tony2001-pinba-engine-changelog.md
related:
  - wiki/concepts/pinba-data-flow.md
  - wiki/concepts/pinba-engine-internals.md
confidence: high
updated: 2026-05-23
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
Both the server (this engine) and the client (PHP extension) ship the same `pinba.proto`.
The fork uses `pinba.pb-c.h` / `pinba.pb-c.cc` (protobuf-C, not C++) for performance.

See: [[pinba-data-flow]], [[pinba-engine-internals]]
