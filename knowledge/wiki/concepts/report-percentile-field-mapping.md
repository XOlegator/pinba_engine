---
title: "Report Percentile Field Mapping (fetch_row numbering)"
type: concept
sources: []
related:
  - wiki/concepts/pinba-data-flow.md
confidence: high
updated: 2026-07-18
---

# Report Percentile Field Mapping (fetch_row numbering)

## How percentile columns bind to fields

Every `reportN_fetch_row` in `src/ha_pinba.cc` maps table columns to values via a
`switch (PINBA_FIELD_INDEX(*field))` with **hardcoded field indices**. Percentile
columns (declared via table `COMMENT='reportN:::90,95,99'`) are handled by the
`default:` branch through `REPORT_PERCENTILE_FIELD(last_field_num, data, cnt)`,
where `last_field_num` must equal the index of the **last named column**
(`index_value`) in the report's reference schema from `default_tables.sql`.
Percentile fields then occupy `last_field_num + 1 .. last_field_num + N`.

The numbering therefore depends on how many key columns the report has:

| Reports | Key columns | `index_value` index | `REPORT_PERCENTILE_FIELD` arg |
|---|---|---|---|
| report1 (script), report2 (server), report3 (hostname), report8 (status), report13 (schema) | 1 | 18 | 18 |
| two-key reports (report4/5/6/9/10/11/14/15/16…) | 2 | 19 | 19 |
| three-key reports (report7/12/17…) | 3 | 20 | 20 |

There is **no compile-time or runtime check** that the numbering matches the
actual table DDL: a wrong index silently stores the wrong value into the field
(`Field::store(const char*)` on a FLOAT column parses the string, yielding
0/garbage plus a `Data truncated` warning that strict-mode clients escalate).

## The report3 off-by-one (2013 → 2.11.5)

`report3_fetch_row` (report *by hostname*) used `case 19: index_value` and
`REPORT_PERCENTILE_FIELD(19, …)` even though report3's schema is identical in
shape to report2 (single key column, `index_value` at 18). Introduced in
upstream commit `03005d7` (Antony Dovgal, 2013-06-18, "add histogram 'view'
support") where report1/report2 correctly received 18; survived every fork and
was still present in v2.11.5.

Observable symptoms on any `report3:::90,95,99` table:

- `index_value` is always NULL (field 18 falls into the `default:` branch and
  fails the percentile range check);
- `p90` receives the **hostname string** → 0/garbage + `Data truncated`
  (aborts `INSERT … SELECT` in strict mode — this broke Pinboard's aggregation);
- `p95` holds the true p90, `p99` holds the true p95.

Proof method: drive identical traffic and compare — the by-hostname table's
p95/p99 exactly equal the by-server table's p90/p95, and only the by-hostname
table has NULL `index_value`.

## Regression coverage

`scripts/percentile_mapping_test.sh` (wired into CI after the UDP ingest steps)
creates percentile variants of report2 and report3, drives them with real
traffic via `scripts/send_pinba_packet.py`, and asserts per row:
`index_value <=> key column` and `0 < p90 <= p95 <= p99`. Reports are lazy —
the script creates and selects from the tables **before** sending packets.

## Gotchas

- Report tables only aggregate packets received after first activation
  (create + first SELECT); percentile values also age out with the stats
  window, so read soon after sending.
- When adding percentile support to a new report or changing a report schema,
  update both the `case N: index_value` index and the
  `REPORT_PERCENTILE_FIELD(N, …)` argument together, and extend the mapping
  test if the report shape is new.
