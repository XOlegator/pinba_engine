---
title: "tony2001/pinba_engine — Original README"
type: source
sources:
  - raw/repos/tony2001-pinba-engine-readme.md
related:
  - wiki/concepts/pinba-data-flow.md
  - wiki/concepts/pinba-pool-model.md
  - wiki/concepts/mysql-plugin-install.md
confidence: high
updated: 2026-05-23
---

# tony2001/pinba_engine — Original README

**What it is:** Original Pinba Engine by Antony Dovgal. Monitoring server that uses MySQL as a query interface. PHP processes send UDP packets; Pinba receives, aggregates, exposes via SQL.

## Key Facts

- **Name meaning**: PHP Is Not A Bottleneck Anymore
- **Original target**: MySQL 5.1 (first version with pluggable storage engines)
- **Transport**: UDP — fire-and-forget, packet loss acceptable
- **Interface**: SQL queries on `ENGINE=PINBA` tables (virtual, in-memory)

## Core Architecture (from README)

1. PHP sends Protobuf/UDP packet at request shutdown
2. Pinba receives on configured port → temporary pool (cyclic buffer)
3. Every `pinba_stats_gathering_period` μs → locks main pool, moves from temp, drops old records
4. Both pools are **fixed cyclic buffers** — newer records overwrite older, never reallocated

## Original Dependencies (no longer needed in the fork)

| Dep | Purpose | Status in fork |
|-----|---------|----------------|
| Google Protocol Buffers | Packet encoding | Replaced by protobuf-C |
| Judy | Hash maps | Replaced by sparsehash |
| libevent | Event loop | Removed (thread pool instead) |
| Hoard allocator | Memory perf | Removed |

## Installation Pattern (original)

Plugin name for `INSTALL PLUGIN`: `libpinba_engine.so`
Tables created from `default_tables.sql` (installed to `/usr/share/pinba_engine/`)

See also: [[pinba-pool-model]], [[pinba-data-flow]]
