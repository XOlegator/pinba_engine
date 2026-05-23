---
title: "Pinba Pool Memory Model"
type: concept
sources:
  - raw/repos/tony2001-pinba-engine-readme.md
  - raw/repos/tony2001-pinba-engine-changelog.md
  - raw/docs/pinba-engine-mysql-sysvars.md
related:
  - wiki/concepts/pinba-data-flow.md
  - wiki/concepts/pinba-engine-internals.md
confidence: high
updated: 2026-05-23
---

# Pinba Pool Memory Model

## Core Design: Fixed Cyclic Buffers

All data storage in Pinba is in pre-allocated fixed-size cyclic buffers.
- **Allocated at init**, never grown, never freed until shutdown
- **Newer records overwrite older ones** when the buffer is full
- **No dynamic allocation** during normal operation — predictable memory usage

## Pool Hierarchy

```
UDP socket
    │ (per-thread reader threads, see pinba_data_job_size)
    ▼
Per-thread Temporary Pools  [pinba_temp_pool_size = 10000 records each]
    │ (every pinba_stats_gathering_period = 10ms)
    ▼ locked merge
Main Request Pool           [pinba_request_pool_size = 1,000,000 records]
    │
    ├─► Timer Pool          [pinba_timer_pool_size = 100,000 timers]
    │
    └─► In-memory Reports   (indexed views over the main pool)
```

## What Happens When Pools Are Full

| Pool | Overflow Behaviour |
|------|--------------------|
| Temp pool | Packet is **dropped** → `lost_tmp_records` counter in `status` table |
| Main pool | Oldest records are silently overwritten |
| Timer pool | Oldest timers silently overwritten |

There is no backpressure to PHP — UDP is fire-and-forget.

## Key System Variables

| Variable | Default | Effect |
|----------|---------|--------|
| `pinba_request_pool_size` | 1,000,000 | Main buffer capacity |
| `pinba_timer_pool_size` | 100,000 | Timer buffer capacity |
| `pinba_temp_pool_size` | 10,000 | Per-thread ingest buffer |
| `pinba_stats_gathering_period` | 10,000 μs | Main pool lock frequency |
| `pinba_stats_history` | 900 s | How long records remain before overwrite |
| `pinba_data_job_size` | 2,048 | Queue depth before processing starts |

## Data Loss Scenarios

1. **Normal**: records expire after `pinba_stats_history` (expected, by design)
2. **MySQL restart**: entire pool cleared (expected, monitoring data is transient)
3. **Temp pool overflow** (`lost_tmp_records > 0`): too many requests per second, increase `pinba_temp_pool_size`
4. **Main pool overflow**: too many unique requests in history window, increase `pinba_request_pool_size`

## Monitoring Pool Health

```sql
SELECT current_temp_pool_size, current_timer_pool_size,
       lost_tmp_records, invalid_packets
FROM pinba.status;
```

- `lost_tmp_records > 0`: packet loss, increase temp pool
- `invalid_packets > 0`: malformed UDP data or wrong protobuf format

See: [[pinba-data-flow]], [[pinba-engine-internals]]
