---
title: "Protobuf Modernization Session — 2026-06-09"
type: source
sources:
  - https://github.com/XOlegator/pinba_extension/pull/13
  - https://github.com/XOlegator/pinba_engine/pull/45
related:
  - wiki/concepts/protobuf-runtime-strategy.md
  - wiki/concepts/pinba-udp-protocol.md
confidence: high
updated: 2026-06-09
---

# Protobuf Modernization Session — 2026-06-09

A working session that modernized protobuf usage in both the PHP extension and
the engine, and analysed whether the two projects should treat protobuf
uniformly. Outcome of the analysis: unify the **contract** (`pinba.proto`), keep
the **runtimes** divergent because each is idiomatic for its host
(C++ protobuf for the MySQL 8 plugin, protobuf-c for the lean PHP C extension).
See [[protobuf-runtime-strategy]].

## Extension changes (XOlegator/pinba_extension PR #13)

- Resynced `pinba.proto` with the generated bindings and the engine contract:
  it was missing `timer_ru_utime` (22) and `timer_ru_stime` (23). Regenerating
  from the stale schema would have silently dropped per-timer rusage.
- Dropped the vendored 2008-era protobuf-c runtime; now links the system
  `libprotobuf-c` (`>= 1.0`) via `pkg-config` in `config.m4`. Bindings
  regenerated with system `protoc-c` and now include `<protobuf-c/protobuf-c.h>`.
- Added a CI **drift guard** that regenerates bindings and fails if the committed
  files diverge from `pinba.proto`.
- Verified the serialized packet is byte-for-byte identical (SHA-256) before and
  after the migration for a deterministic input — the wire contract is unchanged.

## Engine changes (XOlegator/pinba_engine PR #45)

- Removed the dead vendored `src/protobuf-c.h` (protobuf-c 2.0.0), unused since
  the MySQL 8 migration replaced the struct ABI with the hand-written
  `src/pinba.pb-c.h`. Cleaned up the matching README, `debian/copyright` (DEP-5
  stanza plus the now-orphaned BSD-3-Clause license block), and the stale
  clang-format exclusions in CI and `.pre-commit-config.yaml`.
- Documented the C++ protobuf shim and the rationale for using C++ protobuf
  (in `src/pinba.pb-c.cc` and `docs/build.md`).

## Key facts

- **Latest `actions/checkout`** at the time was `v6` (the engine already used it;
  the extension was bumped from `v4`).
- **Fields 22/23** were introduced in the engine on 2013-11-12 by Antony Dovgal
  ("add rusage to timers and timer reports").
- **Debian policy** forbids embedded code copies, which is the packaging reason
  to prefer the system `libprotobuf-c` over a vendored runtime.
- A previously documented claim that the engine "uses protobuf-c, not C++" was
  stale and was corrected: the engine uses C++ protobuf with a shim.
