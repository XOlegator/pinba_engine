---
title: "Opt-in Fractional Percentile Grammar"
type: concept
sources: []
related:
  - wiki/concepts/histogram-boundary-semantics.md
  - wiki/concepts/report-percentile-field-mapping.md
confidence: high
updated: 2026-10-18
---

# Opt-in Fractional Percentile Grammar

Legacy COMMENT suffixes retain prefix parsing: `99.9` still selects p99. Source
inventory of the engine, extension and Pinboard found no declared fractional
tables, but production DDL was not inspected. Compatibility therefore does not
depend on assuming that such tables do not exist.

`:::p=99.9,99.99,100` explicitly selects the new grammar. It accepts percentages
in `(0,100]`, up to nine decimal places, with no signs, whitespace, exponents,
locale separators, non-finite values or trailing junk. Complete tokens are
validated in both CREATE validation and share construction.

Each selector stores its display double and an exact decimal numerator and
denominator. A wide integer intermediate computes `ceil(N * p / 100)` without
binary floating-point errors at integer ranks or overflow for native-sized N.
The supported GCC/Clang 64-bit targets provide the intermediate integer type.
Legacy selectors retain float/floor ranks. Result order and duplicate selectors
are unchanged; the shared histogram's bucket interpolation remains unchanged.

The histogram rank argument now has native count width, avoiding the old
32-bit narrowing at high weighted counts. Empty reports and p100 are bounded.
Per-table histogram maxima are checked before allocation: finite values in
`[1, INT_MAX]`, retaining legacy decimal-to-int truncation. This avoids invalid
float-to-int casts and zero-width bucket calculations from malformed COMMENTs.

Unit tests cover grammar, exact decimal ranks, legacy ranks and invalid maxima.
The UDP/SQL regression creates legacy and fractional reports over 100000 weighted
observations; it verifies distinct p99/p99.9/p99.99, order/duplicates and malformed
DDL rejection. Existing percentile field mapping, histogram and lifecycle
regressions pass under ASan/UBSan. See `docs/tables.md` for public syntax.
