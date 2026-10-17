---
title: "Engine Row References and Filesort"
type: concept
sources: []
related:
  - wiki/concepts/pinba-pool-model.md
  - wiki/concepts/pinboard-architecture.md
confidence: high
updated: 2026-10-17
---

# Engine Row References and Filesort

## Why ORDER BY can return repeated or stale values

The server can sort full row values or sort keys plus engine row references.
Only the latter path calls `handler::position()` while scanning and
`handler::rnd_pos()` to read the sorted rows back. An empty `position()` can
therefore remain hidden during ordinary queries and fail under another sort
plan. On MariaDB 11.8, `SET SESSION max_length_for_sort_data=0` forces the
reference-based path: the original handler returned repeated values or no rows.
This directly affects Pinboard's `ORDER BY ... LIMIT offset, 1` percentile
subqueries on the `request` table, across time, memory, CPU and document size.

## Reference contract for request and timer tables

- Capture the physical pool slot of the successfully fetched row, including
  index lookups. Scan cursors already point to the next row after fetching.
- Encode the saved slot in `ref` with `my_store_ptr()` and decode it with
  `my_get_ptr()` using the same `ref_length`.
- Read that slot exactly, without adding the current `pool->out` or advancing
  scan/index state. Pool expiration can move `out` between reads.
- Return `HA_ERR_RECORD_DELETED` for a missing referenced row, rather than
  silently skipping to another slot or ending the sorted result.
- `active_index` can be invalid during a table scan; random reads must not
  index `this_index[active_index]` unconditionally.
- The final valid pool slot is `size - 1`. Sequential traversal wraps when its
  next cursor reaches `size`; exact reads must not wrap the final valid slot.

The pools remain live and nontransactional: this contract does not provide an
InnoDB-style snapshot or protect against a slot being reused during a query.
Report/tag table random positioning is outside this request/timer fix.

## Regression coverage

`scripts/row_position_test.py` sends uniquely marked requests and compares
forced-reference sorted queries against unsorted snapshots. It checks four
request metrics, both directions, multiple LIMIT offsets and timer lookups by
request ID. With 100-slot pools and a 10-second history, use
`--warmup=80 --warmup-delay=11` to exercise expiration, nonzero `out` and
wraparound. The original plugin fails the sort assertions; the corrected
plugin passes, including the last pool slot.
