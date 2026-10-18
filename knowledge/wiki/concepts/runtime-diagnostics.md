---
title: "Runtime Admission and Memory Diagnostics"
type: concept
sources: []
related:
  - wiki/concepts/request-structure-validation.md
  - wiki/concepts/dictionary-word-lifetime.md
  - wiki/concepts/pinba-pool-model.md
  - wiki/concepts/mysql-plugin-install.md
confidence: high
updated: 2026-10-18
---

# Runtime Admission and Memory Diagnostics

`pinba_runtime.{h,cc}` provides native-width counters and lock-protected live
gauges through additive SHOW GLOBAL STATUS callbacks for MySQL and MariaDB.
`docs/diagnostics.md` defines names, units, balance equations and lower bounds.
Counters reset at initialization, identified by a wall-clock epoch plus monotonic
uptime. Combined snapshots are not transactional.

Datagrams and individual Request nodes are accounted separately. Receive counts
include decode failures and empty datagrams. A full queue no longer silently
skips the remaining recvmmsg batch. Kernel drop sampling uses SO_RXQ_OVFL and
modular cumulative deltas, tolerating old samples and ordinary 32-bit wraps.

Staging preparation rejects, raw capacity rejects and raw allocation rejects
have separate counters. Structural rejects have explicit reasons. Successfully
copied raw rows are compacted across worker reservations only on an allocation
failure path; staging IDs and monotonically increasing request counters are
updated together. Reusable buffers retain their old allocations on realloc
failure, and unpublished slots stay inactive.

The queue-completion regression exposed an early read-pool length reset left in
the C01 ownership change: valid batches skipped decoded-tree destruction. The
reset now occurs only after each root is destroyed. A quiescent queue must return
to zero and completed datagrams must balance enqueued datagrams.

Report-job buffers are prepared before raw/report updates or expiration. Failed
admission cannot create ghost report rows; failed expiration defers the entire
operation without advancing the ring. Expiration retains the collector write
lock until subtraction completes, preventing concurrent report creation from
outgrowing preflight buffers. Timing and SQL latency measurements must include
this locking behavior.

Dictionary accounting includes structs and both owned string copies. Staging,
pool and report struct gauges describe known backing allocations, not complete
heap usage. Persistent indices, report keys/histograms, protobuf trees and
allocator retained memory remain separate. Existing active_reports supplies
per-report rows and CPU; complete per-report allocator attribution is not
invented from these lower bounds.

Real UDP/SQL regressions check balance equations, validation reasons, live
gauges, 64-bit counter helpers, and controlled mixed raw/report/expiration
allocation failures. Checked map insertion cleans up keys/values when admission
allocation fails. See [[dictionary-word-lifetime]] for ownership and sanitizer
integration boundaries.
