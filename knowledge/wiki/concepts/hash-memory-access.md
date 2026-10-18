---
title: "Portable xxHash Memory Reads"
type: concept
sources: []
related:
  - wiki/concepts/dictionary-word-lifetime.md
  - wiki/concepts/cmake-build-system.md
confidence: high
updated: 2026-10-18
---

# Portable xxHash Memory Reads

## Defect and scope

The vendored xxHash implementation enabled direct unaligned access on x86 and
some ARM targets. Its `U32_S`/`U64_S` wrappers were ordinary structs on those
platforms. Reading a member through an unaligned pointer violates the language
alignment contract even when the CPU accepts the load. Aligned typed reads of
arbitrary character buffers also introduce effective-type assumptions.

This was observed in a real UDP/SQL sanitizer run on legacy report keys and
reproduced directly by hashing a buffer at offsets 1 through 7. It is independent
of the Pinba2 t1ha linking problem described in the competitor review.

## Correction and compatibility

`src/xxhash.c` reads 32/64-bit words using fixed-size `memcpy` into a local integer
and performs the same endian conversion. The helpers accept `const void*`;
callers no longer cast external byte buffers to integer pointers. Legacy
alignment dispatch remains, but both paths use the same safe loads.

This is a targeted memory-access correction, preserving the vendored algorithm,
seed, public API, output bytes, and streaming-state layouts. It does not replace
the hash family, add a dependency, or change SQL/protobuf contracts.
Modern upstream xxHash also provides a
[portable memcpy reader](https://github.com/Cyan4973/xxHash/blob/v0.8.3/xxhash.h#L2504-L2513).
Compiler optimization can inline constant-size copies; this correction makes
no end-to-end throughput claim.

## Regression contract

`tests/test_xxhash.cc` freezes 28 hash pairs from the prior implementation using
seeds 0 and 2001 and binary input crossing 16/32-byte block boundaries. It checks
all eight byte alignments, every length from 0 to 256 with three seeds, exact
allocation tails under ASan, and streaming chunks crossing block boundaries.

The new regression fails under UBSan before the correction. Alignment checking
remains enabled after the fix. The tests run in the existing unit/sanitizer CI
jobs. Mixed distribution-server/plugin runs still require the MEM_ROOT manual
poisoning workaround described in [[dictionary-word-lifetime]]; it does not
disable heap bounds, use-after-free, or UBSan alignment diagnostics.
