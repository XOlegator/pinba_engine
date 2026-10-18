---
title: "Reproducible Ingest Workload Validation"
type: concept
sources: []
related:
  - wiki/concepts/runtime-diagnostics.md
  - wiki/concepts/dictionary-word-lifetime.md
  - wiki/concepts/request-structure-validation.md
  - wiki/concepts/mysql-plugin-install.md
confidence: high
updated: 2026-10-18
---

# Reproducible Ingest Workload Validation

`scripts/ingest_benchmark.py` and `docs/benchmarking.md` define a portable,
deterministic regression workload for a disposable server with short history.
The optional pinned PyMySQL driver uses persistent connections so per-query
process creation does not dominate SQL latency. It is not a plugin dependency.

The matrix varies dictionary value/name cardinality, nested Requests, timers per
Request and distinct report fanout. Every window validates raw/timer counts,
report request/time or hit sums and natural expiration. Info time sums retain
the existing millisecond truncation and float conversion tolerance. Fixture
fingerprints must match across compared builds; aliases are not counted as
independent reports. Failed or lost data invalidates the run.

Results include process CPU time, sampled RSS, native diagnostic deltas when
available, SQL raw-plus-report p50/p95/p99, and UDP-to-SQL visibility upper bounds.
Visibility includes observer polling/query delays; SQL timings include client and
wire overhead. Readers stay active during report subtraction and collection.
Run Release builds without sanitizers or test-failure hooks, keep hardware/server
settings/compiler/fixtures fixed and use a fresh server process per build/mode.

Paced finite windows establish regression evidence, not saturation capacity,
production latency guarantees or superiority over another implementation. RSS
includes the database and retained pool/hash/allocator capacity; known byte gauges
are deliberately partial. Compare optional tag GC separately from default catalog
semantics. Concrete measurements and machine-specific artifacts belong in the
analysis document or `knowledge/output/`, rather than portable concept pages.
