---
title: "PHP Extension Test Infrastructure"
type: concept
sources:
  - wiki/sources/test-infra-appstream-session-2026-06-10.md
related:
  - wiki/concepts/php-extension-build.md
  - wiki/concepts/php-extension-api.md
  - wiki/concepts/pinba-udp-protocol.md
confidence: high
updated: 2026-06-10
---

# PHP Extension Test Infrastructure

How the `XOlegator/pinba_extension` test suite and its CI quality gates are built. The
functional tests are PHPT; the surrounding infrastructure adds coverage and memory-safety
checks that PHPT alone does not provide.

## PHPT is the right tool (not archaic)

`.phpt` + `run-tests.php` is the canonical, official test format for a PHP C extension —
php-src itself uses it. It is the only harness that can drive extension load behaviour
(`--SKIPIF--`), INI handling (`--INI--`), startup, and per-test process isolation.

PHPUnit/Pest are **not** appropriate here: the whole extension surface (`pinba_*` functions,
`PinbaClient`) lives in C, there is no userland PHP package to test, and PHPUnit would only
re-load the same extension and assert on the same functions, worse than PHPT.

`run-tests.php` is provided by `phpize`/PHP (it is gitignored, not vendored).

## Test format note: --SKIPIF-- over --EXTENSIONS--

Modern `run-tests.php` has an `--EXTENSIONS--` section, but the version shipped with PHP 8.4
locally does not support it, and the version varies across the 8.2–8.5 matrix. The portable
choice is therefore `--SKIPIF-- <?php if (!extension_loaded("pinba")) print "skip"; ?>`,
which every `run-tests.php` understands. The suite uses `--SKIPIF--`.

## Regression-focused test suite

Beyond the original happy-path tests, the suite asserts the most regression-prone contracts:

- **On-wire format** (the contract with the Pinba Engine): a small top-level protobuf scanner
  in a shared `tests/pinba_wire.inc` decodes `pinba_get_data()` / `PinbaClient::getData()`
  and resolves the string dictionary (field 15) and tag-index fields (12/13/14/20/21), so
  tests assert logical content independent of internal dictionary ordering. Covers
  per-timer rusage (fields 22/23), dictionary **de-duplication** of tag names shared across
  timers, and request-level rusage (`setRusage`, fields 8/9).
- **Replace vs merge/aggregate semantics** — `*_replace` overwrite vs `*_merge`; `setTimer`
  overwrite vs `addTimer` aggregate.
- **Argument validation** — empty tags, numeric tag keys, bad hit_count, rusage arity, and the
  asymmetry where procedural `pinba_timer_add` clamps a negative value to 0 while
  `PinbaClient::addTimer` rejects it. Captured deterministically via `set_error_handler`.

Tests use exact binary fractions (0.25, 0.125, 0.0625) so float wire values are exact.
See [[pinba-udp-protocol]] for the wire fields.

## Unified entry point: tools/run-tests.sh

One build-and-test recipe shared by local dev and CI, so they never drift. It rebuilds the
extension with the flags for the chosen mode and always writes `tests/junit.xml`:

| Mode | What it does |
|------|--------------|
| `test` | plain optimised build (default) |
| `coverage` | gcov-instrumented build; writes `coverage.xml` via `gcovr` |
| `asan` | AddressSanitizer + UBSan build |
| `valgrind` | run each test under Valgrind memcheck (`run-tests.php -m`) |

## CI quality gates

- **C code coverage (gating)** — gcov build, `gcovr` → Cobertura XML uploaded to Codecov +
  artifact, README badge. Roughly 83% of `pinba.c` lines are exercised.
- **Sanitizers ASan + UBSan (gating)** — see below.
- **JUnit reporting** — the matrix `Test` step emits `tests/junit.xml`, published as a per-PHP
  check via `mikepenz/action-junit-report`.
- **Valgrind (informational)** — runs but is non-blocking; PHP is noisy under Valgrind until a
  suppression file is curated.

## ASan must instrument PHP itself (RTLD_DEEPBIND)

Loading an ASan-instrumented extension into a stock PHP fails: PHP loads extensions with
`RTLD_DEEPBIND`, which ASan rejects when only the extension (not PHP) is instrumented. The
extension-only path (preload `libasan` via `LD_PRELOAD`) works only on a PHP built **without**
`RTLD_DEEPBIND` (e.g. some local dev builds); on `setup-php` runtimes it aborts.

The CI therefore builds a minimal PHP CLI instrumented with ASan/UBSan from the official
release tarball (`tools/build-php-asan.sh`, `--disable-all --enable-cli --enable-debug`,
cached between runs) and runs the suite against it. `tools/run-tests.sh` skips the
`LD_PRELOAD` shim when PHP is already instrumented (`PHP_ASAN=1`). This makes ASan a real,
gating memory-safety check.

## Environment gotchas

- **gcovr + configure probes** — `./configure` builds throwaway `conftest` probes; with
  coverage flags they leave orphan `a-conftest.gcno` files with no source that abort `gcovr`.
  Delete `a-conftest.*`/`conftest.*` before reporting.
- **ASLR vs ASan** — Ubuntu 24.04 runners default to `vm.mmap_rnd_bits=32`, which the gcc 13
  ASan runtime cannot map shadow memory against; lower it to 28 before the ASan run.
- **A debug PHP build** (`--enable-debug`) additionally enforces the "Arginfo / zpp mismatch"
  check, catching arginfo declarations that disagree with `zend_parse_parameters`.

## The infrastructure pays off: two real bugs found

The first real ASan/debug-PHP run immediately surfaced two pre-existing bugs in `pinba.c`,
both fixed:

1. **arginfo/zpp mismatch** in `pinba_timers_get()` — it parses an optional `flags` argument
   but declared no arguments in arginfo; the debug build fatals on this.
2. **Use-after-free of timer resources at request shutdown** — `PINBA_G(in_rshutdown)` was set
   *after* the RSHUTDOWN auto-flush, so the flush deleted timer resources from
   `EG(regular_list)` and the executor then freed that list again (double free). Fix: set the
   flag before flushing and skip the manual `regular_list` deletion during shutdown, letting
   the executor own those frees; mid-request `pinba_flush()` is unchanged.

See [[php-extension-build]] for the build system and [[php-extension-api]] for the API.
