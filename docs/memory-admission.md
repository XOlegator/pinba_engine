# Dictionary admission and tag descriptor collection

Existing clients, SQL schemas, and default catalog semantics remain unchanged.
The following **read-only startup variables** are opt-in:

| Variable | Default | Effect |
|---|---|---|
| `pinba_ingest_dictionary_words_limit` | `0` | Refuse a new UDP dictionary string when the allocated word count reaches the limit |
| `pinba_ingest_dictionary_bytes_limit` | `0` | Refuse a new UDP dictionary string when its known byte charge would exceed the limit |
| `pinba_tag_names_limit` | `0` | Refuse a new timer tag name when the descriptor count reaches the limit |
| `pinba_tag_gc_enabled` | `OFF` | Collect unused timer tag descriptors approximately once per second |

Zero limits mean unlimited. Existing strings/names remain usable at a limit.
Dictionary bytes charge `sizeof(pinba_word) + 2 * (stored_length + 1)` per word:
the word struct, its string, and the independently owned map key. Map nodes,
buckets, allocator overhead, decoded protobuf trees, pools and reports are not
included. This is an **admission budget**, not a process RSS or engine-wide cap.

Report configuration can pin dictionary words independently of UDP admission;
DDL is not refused by the UDP word budgets. Pinned configuration counts toward
the observed totals, so the totals may exceed the configured UDP threshold.
Byte and word limits include zero-reference entries awaiting the periodic sweep.
A refused Request may have interned some words or descriptors before the refusal;
those remain accounted and become collectible after staging ownership is released.
No full-map sweep runs on every refused packet.

Dictionary preparation failures reject an entire Request node before raw rows,
timers or report additions are published. With tag limits or collection enabled,
surviving timers' tag names are preflighted at the same stage. Nested Request
nodes have independent outcomes. `Pinba_requests_resource_rejected` counts these
nodes; `Pinba_dictionary_limit_rejected` and `Pinba_tag_limit_rejected` identify
the limiting resource. Separate allocator-failure counters cover later raw/timer
allocation failures; see [runtime diagnostics](diagnostics.md).

## Tag IDs and owners

With collection disabled, historical tag names remain queryable until shutdown.
Enabling collection intentionally changes that catalog retention policy. Choose
it explicitly when consumers can tolerate disappearance of unused catalog rows.
A tag ID never changes while its descriptor lives and is never reused within one
plugin initialization, including after collection. IDs remain in the existing
signed SQL/internal range; exhaustion refuses new names instead of wrapping.

The collector requires all of the following before deleting a descriptor:

- No staging/raw dictionary owner holds its name.
- No active timer-tag report selects its ID.
- No SQL handler holds the descriptor as its current cursor row.

Report construction and SQL pointer access are synchronized with collection by
`collector_lock`; registry access also uses `words_lock`. Cursor pins span
individual read calls and are released on scan/index completion, reset, close,
and handler destruction. Freed IDs leave holes: scans use a live ordered ID
index in collection mode, exact lookups do not silently advance to another row,
and saved row references identify the actual tag ID. The tag hash indexes do
not advertise ordered/reverse iteration to the SQL optimizer; ORDER BY uses a
real sort. Catalog scans are live observations, not transactional snapshots.

A long-lived cursor or configured report can deliberately keep a descriptor
alive. Use a finite `pinba_tag_names_limit` together with collection when a
bounded descriptor count is required. Collection alone does not cap active
cardinality; a limit alone, with collection disabled, can stay exhausted until
restart. No setting promises that the allocator returns released memory to the OS.

## Verification

`tests/test_pinba_dictionary.cc` covers exact budget boundaries, existing-word
identity at saturation, unsigned byte arithmetic and recovery after collection.
`scripts/tag_lifetime_test.py` requires a disposable server with default tables
and history <= 10 seconds. Run it with default settings, collection enabled,
and each limit independently (for example 8 words, 200 known bytes, 4 tag names).
It checks three churn windows, ID holes, ORDER BY with offsets, report selector
ownership, a sleeping table-scan cursor with multiple catalog rows, whole-Request refusal, and recovery.
