---
title: "Dictionary Word Ownership and Reclamation"
type: concept
sources: []
related:
  - wiki/concepts/pinba-pool-model.md
  - wiki/concepts/pinba-data-flow.md
  - wiki/concepts/engine-row-references.md
  - wiki/concepts/histogram-boundary-semantics.md
  - wiki/concepts/hash-memory-access.md
confidence: high
updated: 2026-10-18
---

# Dictionary Word Ownership and Reclamation

## Problem and ownership contract

The 2.11.8 dictionary interned unique strings until plugin shutdown. Limiting
the request-ring capacity did not limit accumulated dictionary values across
successive history windows. A stream of distinct URLs or tag values could keep
growing the dictionary after the corresponding requests expired.

`src/pinba_dictionary.{h,cc}` introduces counted ownership without changing
the protobuf schema, SQL layouts, units, raw retention, or tag IDs. Interning
or looking up a word returns an owned reference. Copying a borrowed pointer
does not transfer ownership; retaining it explicitly does.

| Owner / borrower | Lifetime |
|------------------|----------|
| Staging `pinba_stats_record_ex::words` | Owns one reference per decoded dictionary entry until the batch finishes. Rejected and capacity-dropped records are released as well |
| Raw `pinba_stats_record::dictionary_words` | Owns the packet dictionary, keeping request tag names/values and every timer value alive until report deletions and timer clearing finish |
| Request-tag arrays and timer values | Borrow from the parent raw dictionary; a timer cannot outlive its request |
| Report condition names/values | Own references until that report is destroyed, including while there are no matching raw requests |
| Request-tag report selectors | Own references independently of raw expiration, preserving pointer identity for future matching packets |
| Report result keys/strings | Copy their strings; they do not retain borrowed word pointers |
| N-tag report scratch arrays | Borrow while processing a retained raw record under the report lock; their stale slots are not owners |

Duplicate dictionary entries may resolve to the same word. Each retained entry
has a corresponding release; treating the array as a set would miscount these
owners. Per-record pointer buffers are reused after expiration, but their
counts are reset and they no longer retain old strings.

## Synchronization and collection

Word references use atomic counts. Lookup and acquisition occur under the
dictionary read lock; insertion upgrades to the write lock and retains the
word **before** restoring the read lock. Otherwise a newly inserted zero-count
word could be collected during the upgrade gap.

The stats thread sweeps zero-reference words approximately once per second,
even when no packets arrive. Collection holds only `words_lock` exclusively,
and uses an erase-safe map traversal rather than deleting the cursor key
before calling `pinba_map_next()`. A release does not free a word immediately.
Atomic release/acquire ordering and lock-protected acquisition keep zero-count
reclamation separate from concurrent legitimate users.

Raw ownership is released after all report-subtraction jobs and timer clearing
have completed. SQL raw readers observe the existing live-table locking and
active-record rules; this does not introduce transactional snapshots. Staging
cleanup runs after worker barriers and covers invalid, nested, and dropped
requests. Timer merging uses accepted staging-record counts, **not datagram
counts**: validation failures and nested batches make those different.

## Report registration is also ownership

Report `use_cnt` counts registered SQL tables sharing that report, rather than
cached handler instances. Reopening a table does not acquire another report
owner. Dropping an alias preserves the report while another table refers to it.
Dropping the final table releases its conditions and request-tag selectors.

Both table/report registries are detached consistently, including for inactive
reports. The old DROP path freed an index copy without removing its
`tables_to_reports` entry, allowing a later double-free at shutdown. Report
deletion now follows registry-lock-before-share-mutex ordering, consistent
with report creation; it does not recursively reacquire those registry locks.

## Observable behavior and limits

`status.dictionary_size` now falls after expiration and final report deletion.
It includes configured selectors that still have owners, and may include
zero-reference words awaiting the next sweep. Existing SQL column positions
and meanings remain intact; no migration or new client protocol is required.

Opt-in startup word/known-byte admission budgets and a tag-name count limit
refuse new resources before publishing the Request. Zero defaults preserve
unlimited admission. Existing words and descriptors remain usable at saturation;
DDL word pins are exempt from UDP admission but contribute to observed totals.
Known bytes charge structs and two string copies, not hash nodes or allocator
metadata. See `docs/memory-admission.md` for the exact scope and rejection counters.

The separate timer-tag descriptor registry remains persistent by default.
`pinba_tag_gc_enabled` explicitly opts into collecting descriptors without a
staging/raw name owner, active report selector, or SQL cursor pin. IDs are
monotonic and never reused within one initialization. An ordered live-ID index
supports scans across collected holes; exact lookups and saved row references
continue to identify the requested ID. Tag hash indexes no longer advertise
ordered or reverse iteration. Registry lookups use `words_lock`; collection also
holds `collector_lock` to exclude borrowed-pointer users and report construction.

These are **not engine-wide memory limits**. Pools, decoded trees, report keys,
hash buckets and allocator RSS have separate costs. Word collection and optional
descriptor collection do not promise decreasing RSS. A configured selector or
long cursor can keep its descriptor alive. Finite name admission limits bound
registry count even when such owners prevent reclamation. With GC disabled,
a name limit can remain exhausted until shutdown.

Costs include atomic ownership, reusable raw dictionary pointer buffers,
linear dictionary sweeps, and an additional ordered ID index only when tag GC
is enabled. Measure throughput and tail latency with the intended workload.

## Verification

`tests/test_pinba_dictionary.cc` covers admission boundaries/recovery, shared identity, raw/staging ownership,
report pins, duplicate entries, truncation, high-cardinality churn, partial
selector arrays, and concurrent lookup/intern/release/collection.

`scripts/dictionary_lifetime_test.py` requires a disposable database with
`pinba_stats_history <= 10` and the default tables. It checks multiple windows,
wraparound, simultaneous SQL readers and report DDL, shared report aliases,
timer-only packets, semantic validation failures, nested requests, and capacity
drops for a small request ring. Unique values return to the baseline after
their final owners expire. The baseline 2.11.8 plugin fails that reclamation
check. Existing smoke, percentile mapping and row-reference regressions remain
applicable; see [[histogram-boundary-semantics]].

### Sanitizer integration boundary

A sanitizer-instrumented plugin loaded into a distribution MariaDB can activate
manual poisoning in inline `my_valgrind.h`/MEM_ROOT helpers while the host server
is not instrumented to perform matching unpoisoning. For that mixed build, the
focused UDP/SQL run uses ASan `allow_user_poisoning=0` with heap bounds and UAF
checks still active. Unit tests keep normal poisoning and LeakSanitizer enabled;
the host-server run disables leak reporting for the uninstrumented server.

The focused dictionary and histogram paths pass ASan/UBSan. Extending that run
to legacy reports originally exposed an alignment diagnostic in vendored
`xxhash.c` (`XXH_readLE64_align` on an unaligned report key). The follow-up
correction replaces integer/struct loads with fixed-size `memcpy`, preserving
legacy hashes; see [[hash-memory-access]]. Sanitizer results cover the exercised
paths and do not prove that every untested engine path is free of undefined
behavior.

`scripts/tag_lifetime_test.py` checks opt-in descriptor collection, report/cursor
pins, monotonic IDs, SQL sorting with holes, default persistent names, independent
word/byte/name limits and post-expiration admission recovery.
