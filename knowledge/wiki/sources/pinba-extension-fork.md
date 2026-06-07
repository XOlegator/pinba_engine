---
title: "PHP Pinba Extension — XOlegator Fork"
type: source
sources:
  - raw/repos/php-pinba-extension.md
related:
  - wiki/concepts/php-extension-api.md
  - wiki/concepts/php-extension-build.md
  - wiki/concepts/php-extension-packaging.md
  - wiki/concepts/php-version-monitoring.md
  - wiki/concepts/php-pinba-configuration.md
confidence: high
updated: 2026-06-07
---

# PHP Pinba Extension — XOlegator Fork

**Upstream:** tony2001/pinba_extension on GitHub. Final upstream release: 1.1.2 (Aug 2020).
**Fork:** XOlegator/pinba_extension — modernized for PHP 8.2–8.5 with CI, PHPT tests, and packaging automation.
**Fork version:** 1.1.2 (fork; release-please tracking v1.2.0 as next release)

## Purpose

The upstream extension is effectively unmaintained since 2020 and does not support PHP 8.2+.
The fork goal is to:
- Restore compatibility with all currently supported PHP branches (8.2, 8.3, 8.4, 8.5)
- Add CI matrix to catch per-version breakage early
- Add Debian packaging automation for Ubuntu 24.04 and 26.04
- Maintain stable API and protocol compatibility with upstream

## Repository Structure

```
pinba_extension/
├── pinba.c                  — main extension: PHP functions, lifecycle, UDP send
├── pinba.pb-c.c             — protobuf-c generated serializer for Pinba__Request
├── protobuf-c.c             — bundled protobuf-c runtime (no external dep)
├── pinba.pb-c.h             — protobuf struct definitions
├── php_pinba.h              — PHP extension header (PHP_PINBA_VERSION, globals struct)
├── config.m4                — PHP build system config (PHP_ARG_ENABLE, mallinfo check)
├── tests/                   — PHPT test suite (8 tests)
├── .github/
│   ├── php-versions.json    — current CI support matrix (generated, updated by automation)
│   ├── packaging-matrix.json — Debian packaging target matrix
│   ├── workflows/
│   │   ├── ci.yml                      — build + PHPT test across PHP version matrix
│   │   ├── lint.yml                    — actionlint + markdownlint quality gates
│   │   ├── discover-php-versions.yml   — weekly auto-update of php-versions.json
│   │   ├── pr-title-conventional.yml   — enforce conventional commit PR titles
│   │   └── release-please.yml          — automated releases via release-please
│   └── scripts/
│       └── update-php-support-matrix.php — parses php.net, updates php-versions.json
└── docs/
    └── packaging.md         — Debian packaging plan and design constraints
```

## Key Constants

- `PHP_PINBA_VERSION` = `"1.1.2"` (defined in `php_pinba.h:34`, updated by release-please)
- `PINBA_COLLECTOR_DEFAULT_PORT` = `"30002"` — default UDP destination port
- `PINBA_COLLECTORS_MAX` = 8 — maximum number of simultaneous target servers
- `PINBA_FLUSH_ONLY_STOPPED_TIMERS` = flag for `pinba_flush()`
- `PINBA_TIMERS_STOP_ALL` = flag for `pinba_timers_get()`

## INI Directives

```ini
pinba.server           ; NULL default — host:port or comma-separated list (max 8)
pinba.enabled = 0      ; must be set to 1 to send anything
pinba.auto_flush = 1   ; flush at request shutdown (RSHUTDOWN)
pinba.resolve_interval = 60  ; DNS cache TTL for collector address
```

DNS resolution is cached: the collector address is resolved at most once per
`resolve_interval` seconds, even for multiple requests.

## Changes from Upstream tony2001/1.1.2

- `pinba_hash_sort()`: replaced deprecated `zend_hash_sort()` with a manual
  `SPL`-style comparison loop to fix PHP 8.0+ type signature incompatibility
- `memory_footprint` stat: replaced deprecated `mallinfo()` with `mallinfo2()`
  (for glibc 2.33+), with compile-time fallback for older glibc
- `pinba_timer_add()`: restored missing parameter for pre-completed timers
- PHPT test suite added (8 tests for core extension behavior)
- CI: GitHub Actions matrix across PHP 8.2/8.3/8.4/8.5
- Governance: CONTRIBUTING.md, CODE_OF_CONDUCT.md, release-please automation

## PHPT Test Suite

| Test File | Coverage |
|-----------|---------|
| `001_extension_loaded.phpt` | `extension_loaded('pinba')` is true |
| `002_ini_set.phpt` | `ini_set`/`ini_get` round-trip for all 4 directives |
| `003_timers.phpt` | `pinba_timer_start`, `pinba_timer_stop`, elapsed time range |
| `004_info_and_tags.phpt` | `pinba_get_info()` structure, `pinba_tag_set`/`pinba_tags_get` |
| `005_client_get_data.phpt` | `pinba_get_data()` returns array with expected keys |
| `006_flush_reset.phpt` | `pinba_flush()` and `pinba_reset()` do not crash when disabled |
| `007_stopped_only_data.phpt` | `PINBA_FLUSH_ONLY_STOPPED_TIMERS` flag behavior |
| `008_udp_send.phpt` | `pinba.enabled=1` with mock server — no crash, no output |

Tests run with `make test TESTS=tests/` as part of CI.

See: [[php-extension-build]], [[php-extension-api]], [[php-pinba-configuration]]
