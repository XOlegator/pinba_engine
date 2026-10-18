---
title: "Histogram Upper-Boundary Semantics"
type: concept
sources: []
related:
  - wiki/concepts/report-percentile-field-mapping.md
  - wiki/concepts/pinba-pool-model.md
confidence: high
updated: 2026-10-18
---

# Histogram Upper-Boundary Semantics

`pinba_update_histogram()` in `src/pinba.h` maps a nonnegative duration into
`pinba_histogram_size` buckets. Values at or above `histogram_max_time` belong
to the **last** bucket. The same rule applies when deleting expired samples.
For aggregated timers, divide total duration by the signed update weight's
absolute value before choosing the bucket, then add/remove that weight.

The 2.11.8 implementation handled only values strictly above the maximum this
way. At the exact maximum, division produced the bucket count itself and an
overflow fallback assigned bucket zero. With maximum 10 seconds and 1024
buckets, a ten-second sample therefore appeared alongside zero-duration
samples. The corrected updater saturates at the last bucket, including its
rounding fallback. Interior values, SQL column positions, COMMENT syntax, units,
and the percentile interpolation algorithm are unchanged.

`tests/test_pinba_histogram.cc` exercises the real updater: below/at/above the
upper edge, weighted add/delete, zero and an interior bucket. The edge and
weighted tests fail on the old implementation. `scripts/histogram_boundary_test.py`
checks actual UDP ingestion, histogram SQL output and p100 on a disposable
server; it isolates traffic with a request-tag filter and removes its tables.

Corrected percentile results for samples exactly on the upper edge are an
intentional bug fix. Separate raw-table percentile calculations in Pinboard
do not use this histogram and should not be changed as part of this fix.
