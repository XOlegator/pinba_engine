---
title: "Knowledge Base Overview"
type: overview
sources:
  - raw/repos/tony2001-pinba-engine-readme.md
  - raw/docs/pinba-udp-protocol-proto.md
  - raw/repos/php-pinba-extension.md
  - raw/docs/pinba-engine-mysql-sysvars.md
  - raw/docs/pinba-default-tables.md
  - raw/docs/mysql-plugin-api-install.md
  - raw/docs/xolegator-fork-build-docker.md
related:
  - wiki/index.md
  - wiki/concepts/pinba-data-flow.md
  - wiki/concepts/mysql-plugin-abi.md
  - wiki/concepts/copr-rpm-packaging.md
  - wiki/concepts/engine-rpm-packaging.md
confidence: high
updated: 2026-10-18
---

# Pinba Engine — Knowledge Base Overview

## What This Project Is

**Pinba Engine** is a MySQL storage engine plugin that acts as a monitoring server for PHP applications.
PHP processes send performance data (request times, memory, timers) via UDP; the engine receives,
aggregates, and exposes this data as SQL-queryable virtual tables.

This repository is a **modernized fork** of the original `tony2001/pinba_engine` (MySQL 5.1, 2009-2016),
updated to support MySQL 8.0/8.4 and MariaDB 10.11/11.8 with a CMake+C++23 build
system. The implementation and packaging matrix are authoritative; historical raw
documents describe earlier releases.

## System Architecture

```
PHP App (pinba ext) →[UDP:30002]→ MySQL+Pinba Engine →[SQL]→ Pinboard Admin UI
```

Full chain: `PHP pinba extension` → `UDP:30002` → `Pinba Engine (ha_pinba.so)` inside MySQL →
`ENGINE=PINBA tables in pinba database` → `Pinboard (Symfony admin app)` → developer's browser.

## Core Technical Insights

### 1. The ABI Rule (most critical)
MySQL 8.0 and 8.4 plugins are **not interchangeable**. MariaDB also needs its own
flavor and matching server-series headers. Separate builds and image channels
exist for each supported ABI.
See [[mysql-plugin-abi]].

### 2. In-Memory Only
All Pinba data lives in RAM. MySQL restart = data gone. This is by design — it's a monitoring
buffer, not a database. `pinba_stats_history` (default 900s) controls the window.
See [[pinba-pool-model]].

### 3. UDP Fire-and-Forget
The PHP extension sends metrics asynchronously. Packet loss is acceptable.
Wrong server address = silent data loss, no PHP error, no timeout.
See [[pinba-data-flow]], [[php-pinba-configuration]].

### 4. Timer Encoding Is Non-Obvious
Timers in the protobuf message use parallel arrays + a shared string dictionary.
Understanding this is needed to interpret timer data in `timertag` and `timer` tables.
See [[pinba-udp-protocol]].

### 5. Docker: protobuf Delivery Depends on the Channel
Each runtime must have the protobuf implementation used by its plugin build.
Some channels deliver a shared library; others link protobuf statically. Do not
assume that copying libprotobuf is the correct solution for every runtime.
See [[docker-build-strategy]].

### 6. PPA Publication Is Fully Automated
Source packages for Launchpad PPA are built and uploaded from GitHub Actions.
The workflow signs the source package, uploads via Launchpad FTP with `passive_ftp = 1`
and a retry loop, and uses explicit source refs for manual rebuilds.
When a bad upstream tarball version was burned in the PPA, the correct recovery was to
cut a new upstream release (`v2.2.1`) rather than inventing a pseudo-version.
See [[github-actions-ppa]].

### 7. MySQL Version Updates Trigger Full Release Cycle Automatically
A weekly GitHub Actions workflow detects new MySQL patch versions for the PPA target suites
(`noble`, `resolute`). When a change is detected, it re-extracts vendor headers, updates
`.github/mysql-versions.json` (including `debian_revision`), opens a PR, and enables
auto-merge. When that PR lands, `ppa-build` and `docker` workflows fire automatically,
publishing new packages to Launchpad and Docker Hub without manual intervention.
See [[github-actions-mysql-version-monitor]].

### 8. RPM Distribution via COPR
Alongside the Debian/PPA track, the stack is packaged as `.rpm` through Fedora COPR
(`xolegator/pinba`), for Fedora 43/44 and EPEL 9/10 (AlmaLinux/Rocky/RHEL); a release
auto-publishes to both the PPA and COPR from one tag. The **extension** builds `php-pinba`
against each chroot's distro-native PHP (no Remi at build/run time — Remi's own repo now owns
the multi-version story since fork adoption, issue #58). See [[copr-rpm-packaging]].

### 9. Engine RPM: Offline Build Against Vendored Server Headers
The **engine** RPM (`pinba-engine-mysql` / `pinba-engine-mariadb`) is the counterpart, but
fundamentally different from the extension: a storage-engine plugin compiles against the DB
server's *source* headers (`sql/handler.h`), which no `-devel` ships — so the minimal header
subset is **vendored in-tree and travels in the tarball**, making the mock build fully offline.
One flavor per SRPM (they share `ha_pinba.so` and `Conflicts`); the vendored series is pinned
**per chroot** to match its runtime server (ABI — e.g. Fedora 43 = MariaDB 10.11, Fedora 44 =
11.8). Builds for `x86_64` and `aarch64`. See [[engine-rpm-packaging]].

## Key Configuration Points

| What | Where | Critical Setting |
|------|-------|-----------------|
| PHP extension | `php.ini` | `pinba.enabled = 1` (off by default!) |
| PHP server address | `php.ini` | `pinba.server = "host:port"` |
| Pinba listen port | `my.cnf [mysqld]` | `pinba_port = 30002` |
| Data retention | `my.cnf [mysqld]` | `pinba_stats_history = 900` |
| Docker MySQL series | Dockerfile / tag | must match running MySQL version exactly |

## Project Status

Active modernization fork. At the 2026-10-18 audit, the released baseline is
**2.11.8** (`CMakeLists.txt`); this is a dated observation, not a second version
registry. Builds cover MySQL 8.0/8.4 and MariaDB 10.11/11.8. Docker channels are
`8.0`, `8.4`, `mariadb-10.11`, and `mariadb-11.8`; `latest` follows `8.4`.
The old `8.4-lts*` tags are no longer updated. See [[docker-tag-strategy]].

Launchpad PPA and Copr packaging, SQL integration checks, coverage, and CodeQL
are maintained alongside the plugin. The MySQL version monitor tracks server
header changes; see [[github-actions-mysql-version-monitor]].

Memory is not bounded solely by the configured raw request count: temporary
pools and the timer pool can grow, and reports and dictionaries allocate
dynamically. See [[pinba-pool-model]] for capacity, expiration, and monitoring
semantics rather than assuming all storage is a fixed ring buffer.

## Knowledge Gaps

See [[index]] for the current list of topics not yet covered in this wiki.
