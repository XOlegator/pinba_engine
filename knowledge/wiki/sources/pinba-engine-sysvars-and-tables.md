---
title: "Pinba Engine — MySQL System Variables & Table Schema"
type: source
sources:
  - raw/docs/pinba-engine-mysql-sysvars.md
  - raw/docs/pinba-default-tables.md
related:
  - wiki/concepts/pinba-engine-internals.md
  - wiki/concepts/pinba-pool-model.md
  - wiki/concepts/pinba-report-tables.md
confidence: high
updated: 2026-07-16
---

# Pinba Engine — MySQL System Variables & Table Schema

## MySQL System Variables

All are READONLY — require MySQL restart. Set in `[mysqld]` in `my.cnf`.

| Most Important Variables | Default | Notes |
|--------------------------|---------|-------|
| `pinba_port` | 30002 | UDP listen port |
| `pinba_address` | NULL | Listen IP; NULL = all interfaces |
| `pinba_stats_history` | 900 | Seconds of history kept (15 min default) |
| `pinba_request_pool_size` | 1000000 | Main cyclic buffer size (RAM) |
| `pinba_timer_pool_size` | 100000 | Timer cyclic buffer size |
| `pinba_temp_pool_size` | 10000 | Per-thread temp buffer |
| `pinba_stats_gathering_period` | 10000 | Main pool lock interval (μs) |
| `pinba_data_job_size` | 2048 | Queue depth before processing |
| `pinba_histogram_size` | 1024 | Median computation resolution |
| `pinba_log_level` | ERROR\|WARN\|NOTICE | Bitmask |

## Table Categories

### Raw data (direct pool access)
- `request` — one row per PHP request in pool
- `timer` — linked to request_id
- `timertag` — timer→tag associations
- `tag` — tag name dictionary
- `info` — global totals
- `status` — engine health (pool sizes, lost packets, invalid packets)

### Pre-aggregated reports (18 tables)
Grouped by combinations of: hostname, server_name, script_name, status, schema.

Each report table has columns: req_count, req_per_sec, req_time_total/percent/per_sec,
ru_utime/stime totals, traffic, memory_footprint, req_time_median, index_value (group key).

In the current maintained fork, the default schema uses `varchar(64)` for
`hostname` and `server_name` dimensions in the built-in report tables that store
those fields.

### status table — important for monitoring

```sql
SELECT * FROM pinba.status;
```

Shows: current pool fill levels, `lost_tmp_records` (packets dropped due to full temp pool),
`invalid_packets` (malformed UDP), `invalid_request_data`.

## Data Lifetime

All data is **in RAM only**. MySQL restart = all Pinba data gone. This is by design.
`pinba_stats_history` controls how old records stay in the cyclic buffer.

See: [[pinba-pool-model]], [[pinba-report-tables]], [[pinba-engine-internals]]
