---
title: "Pinba Data Flow — End to End"
type: concept
sources:
  - raw/repos/tony2001-pinba-engine-readme.md
  - raw/docs/pinba-udp-protocol-proto.md
  - raw/repos/php-pinba-extension.md
  - raw/docs/pinba-engine-mysql-sysvars.md
related:
  - wiki/concepts/pinba-udp-protocol.md
  - wiki/concepts/pinba-pool-model.md
  - wiki/concepts/php-pinba-configuration.md
confidence: high
updated: 2026-07-16
---

# Pinba Data Flow — End to End

## Complete Pipeline

```
PHP request executes
       │
       │ (PHP pinba extension tracks timers, measures req_time, memory, etc.)
       │
       ▼ at request shutdown
pinba_extension sends UDP packet (protobuf-encoded Pinba.Request)
       │
       │ UDP → host:30002
       │
       ▼
Pinba Engine UDP listener thread
       │
       ├─► decodes protobuf packet
       ├─► validates fields
       ├─► stores hostname/server_name up to the engine's current 64-character limit
       └─► adds to per-thread temporary pool (cyclic buffer)
                     │
                     │ every pinba_stats_gathering_period (10ms default)
                     ▼
              Main pool lock
              ├─► moves records from temp pool → main pool
              ├─► drops records older than pinba_stats_history (900s)
              └─► updates in-memory report indexes
                     │
                     ▼
              MySQL query (Pinboard or direct)
              SELECT * FROM pinba.report_by_script_name
              SELECT * FROM pinba.request WHERE ...
```

## What Can Go Wrong (and Is Silent)

| Problem | Symptom | Detection |
|---------|---------|-----------|
| `pinba.enabled = 0` | No data, no error | `php -r "var_dump(pinba_get_info());"` |
| Wrong `pinba.server` | No data, no error | `pinba.status` table: invalid_packets stays 0 |
| Wrong port mapping in Docker | No data | check `netstat -ulnp \| grep 30002` |
| Temp pool overflow | Dropped packets | `SELECT lost_tmp_records FROM pinba.status` |
| MySQL restart | All data lost | Expected — data is in RAM |

## Latency and Reliability

- UDP → no round-trip, no blocking, no error feedback
- Acceptable packet loss rate: ~0-1% under normal load
- If Pinba server unreachable: PHP request completes normally, data lost silently
- Performance cost on PHP side: negligible (async UDP send at request end)

## Data Retention

Only data within the `pinba_stats_history` window (default 900s = 15 min) is available.
Pinboard's `aggregate` service reads and stores summaries to its own persistent DB.

See: [[pinba-pool-model]], [[pinba-udp-protocol]], [[php-pinba-configuration]]
