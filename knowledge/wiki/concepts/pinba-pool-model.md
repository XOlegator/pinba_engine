---
title: "Pinba Pool Memory Model"
type: concept
sources:
  - raw/repos/tony2001-pinba-engine-readme.md
  - raw/repos/tony2001-pinba-engine-changelog.md
  - raw/docs/pinba-engine-mysql-sysvars.md
  - https://github.com/XOlegator/pinba_engine/blob/1ec531f5c51de444b7e931cbf1a5406c042ce34b/src/pool.cc
  - https://github.com/XOlegator/pinba_engine/blob/1ec531f5c51de444b7e931cbf1a5406c042ce34b/src/main.cc
related:
  - wiki/concepts/pinba-data-flow.md
  - wiki/concepts/engine-row-references.md
  - wiki/concepts/dictionary-word-lifetime.md
confidence: high
updated: 2026-10-18
---

# Pinba Pool Memory Model

## Storage is a mix of fixed and growable pools

The generic `pinba_pool` tracks capacity, growth increment, optional limit, and
`in`/`out` positions. `pinba_pool_grow()` reallocates storage and, for a wrapped
ring, moves its live tail. It is incorrect to describe all Pinba memory as
preallocated, fixed-size, or allocation-free during ingestion.

| Storage | Capacity and full-pool behavior |
|---------|---------------------------------|
| Raw request ring | Configured by `pinba_request_pool_size`; normal ingestion does not grow it. One slot is reserved to distinguish full from empty. If insufficient slots are free, **new requests are dropped** and `lost_tmp_records` increases; live oldest records are not silently overwritten |
| Timer ring | `pinba_timer_pool_size` is the initial capacity. `timer_pool_add()` grows it when a request needs more room, preserving live timers and adjusting affected request offsets |
| Per-worker decoded-packet buffers | Two alternating sets of linear buffers; initialized by `pinba_temp_pool_size`, grown by `pinba_pool_push()`, optionally capped by `pinba_temp_pool_size_limit` |
| Per-worker temporary records | Growable staging pools used after decoding, including expansion of nested requests. Their counts are reset after transfer |
| Reports, tag arrays, dictionaries | Separately allocated memory; request-ring capacity alone is not an engine-wide memory budget |

Dictionary words now have counted owners and are reclaimed after raw expiration
and final report release; see [[dictionary-word-lifetime]]. The raw request owns
its packet's word-pointer array so both request tags and timer values remain
valid. This does not make all report/tag registries or allocator capacity bounded.

The timer growth increment is `PINBA_TIMER_POOL_GROW_SIZE` (2,621,440 slots),
possibly increased for a large incoming batch. This can produce substantial
capacity growth; the initial timer size is not a maximum. Per-slot tag arrays
are also reallocated as needed. Expiration frees logical records, not necessarily
their backing allocation or allocator-resident RSS.

## Ingestion and expiration

UDP readers decode protobuf into one set of per-worker packet buffers. The data
thread swaps read/write sets, builds temporary records, copies accepted records
to the raw request ring, adds timers, and updates incremental reports. Work is
coordinated with collector/timer/report locks and worker barriers.

The stats thread expires requests older than `pinba_stats_history`, removes
their contributions from reports, clears their timers, and advances ring
positions. There is no backpressure to PHP: UDP send success does not guarantee
ingestion.

## Configuration defaults

Verified against the system-variable declarations in `src/ha_pinba.cc`:

| Variable | Declared default | Meaning |
|----------|---------|---------|
| `pinba_request_pool_size` | 1,000,000 | Fixed request-ring slots, including its reserved slot |
| `pinba_timer_pool_size` | 100,000 | Initial timer-ring capacity |
| `pinba_temp_pool_size` | 10,000 | Initial slots per staging pool, per worker |
| `pinba_temp_pool_size_limit` | 0 | The pool interprets zero as unlimited, but the system-variable declaration also specifies minimum 1000; the server can clamp the effective value |
| `pinba_stats_gathering_period` | 10,000 microseconds | Scheduled collection/expiration cadence, not a guarantee under contention |
| `pinba_stats_history` | 900 seconds | Expiration window |
| `pinba_data_job_size` | 2,048 | Declared packet-processing batch setting; not total queue capacity |

Worker count is derived from online processors, with a fallback for small or
unavailable counts. Total staging memory therefore scales with worker count,
not just one configured pool size.

During the MariaDB 11.8.6 audit, the server warned that the declared zero
`temp_pool_size_limit` was adjusted to 1000. Check
`SELECT @@global.pinba_temp_pool_size_limit` rather than assuming the declared
default is the effective setting. This audit does not change that legacy
configuration behavior.

## Monitoring limitations

`pinba.status.current_timer_pool_size` reports allocated timer capacity, not
the number of live timers. `dictionary_size` reports interned string count.
At the 2.11.8 baseline, `current_temp_pool_size` is a placeholder returning zero;
it cannot establish that staging buffers are empty.

`lost_tmp_records` counts requests rejected when copying staging records into
the full raw ring. It is not a complete count of every UDP/kernel/staging loss.
The collector also logs received/dropped/decode counters. Some validation-error
increments are disabled in the legacy conversion path, so `invalid_packets=0`
does not prove that every received packet was valid.

Restart clears all engine-resident statistics. Pinboard persists its own
historical snapshots separately. See [[pinba-data-flow]] and
[[engine-row-references]] for live raw-table behavior.
