---
title: "Session: Test Infrastructure Modernization + AppStream Packaging (2026-06-10)"
type: source
sources: []
related:
  - wiki/concepts/php-extension-test-infrastructure.md
  - wiki/concepts/appstream-metainfo-packaging.md
  - wiki/concepts/php-extension-build.md
  - wiki/concepts/debian-ppa-packaging.md
confidence: high
updated: 2026-06-10
---

# Session: Test Infrastructure Modernization + AppStream Packaging

Working session of 2026-06-10 spanning two repositories. Sourced from the merged PRs
listed below (no `raw/` document).

## Pinba Engine — AppStream packaging metadata

**PRs:** `XOlegator/pinba_engine#46` (fix), `#47` (release 2.6.1).

KDE Discover showed `pinba-engine-common` with "Author unknown" and "license unknown".
Root cause: the AppStream component in `debian/pinba-engine.metainfo.xml` had no `<pkgname>`,
so it was not bound to the installed package; the metadata existed but never reached the
package card. Fixes:

- Added `<pkgname>` to the common component and added separate `addon` metainfo for
  `pinba-engine-mysql-8.0` / `-8.4`, installed per package in `debian/rules`.
- The PPA workflow now injects the packaged version into every `<releases>` list and runs
  `appstreamcli validate`.
- Released as 2.6.1 (release-please).

See [[appstream-metainfo-packaging]].

## Pinba Extension — regression tests

**PRs:** `XOlegator/pinba_extension#14` (per-timer rusage), `#15` (wire format, replace
semantics, validation).

After a public-API audit (24 procedural functions + 18 `PinbaClient` methods) the suite was
extended from happy-path only to lock the regression-prone contracts: on-wire format with a
shared protobuf-decoding helper (`tests/pinba_wire.inc`), dictionary de-duplication, per-timer
and request-level rusage, replace-vs-merge/aggregate semantics, and argument validation.

## Pinba Extension — test infrastructure modernization

**PR:** `XOlegator/pinba_extension#16`.

Verdict on the "PHPT is archaic" premise: false — PHPT is the correct harness; the gap was the
surrounding infrastructure. Added `tools/run-tests.sh` (test/coverage/asan/valgrind), C code
coverage (gcov/gcovr → Codecov, ~83% of `pinba.c`), JUnit reporting, a gating ASan+UBSan run
against a purpose-built instrumented PHP (`tools/build-php-asan.sh`), and an informational
Valgrind run. PHPUnit and an `--EXTENSIONS--` migration were deliberately not done.

Making ASan real immediately found and fixed two pre-existing bugs in `pinba.c`: a
`pinba_timers_get()` arginfo/zpp mismatch (caught by the debug PHP build) and a
use-after-free of timer resources during request shutdown (caught by ASan).

See [[php-extension-test-infrastructure]].

## Key takeaways

- Software centres read Author/License from AppStream metainfo bound via `<pkgname>`, not from
  `debian/control` / `debian/copyright`.
- A real sanitizer gate for a PHP extension requires instrumenting PHP itself, because stock
  PHP loads extensions with `RTLD_DEEPBIND`, which ASan rejects.
- A debug PHP build (`--enable-debug`) enforces the "Arginfo / zpp mismatch" check; an
  instrumented PHP catches memory-safety bugs that ordinary functional tests cannot see.
