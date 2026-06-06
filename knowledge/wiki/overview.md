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
confidence: high
updated: 2026-06-06
---

# Pinba Engine — Knowledge Base Overview

## What This Project Is

**Pinba Engine** is a MySQL storage engine plugin that acts as a monitoring server for PHP applications.
PHP processes send performance data (request times, memory, timers) via UDP; the engine receives,
aggregates, and exposes this data as SQL-queryable virtual tables.

This repository is a **modernized fork** of the original `tony2001/pinba_engine` (MySQL 5.1, 2009-2016),
updated to support MySQL 8.0 and 8.4 LTS with a CMake+C++23 build system.

## System Architecture

```
PHP App (pinba ext) →[UDP:30002]→ MySQL+Pinba Engine →[SQL]→ Pinboard Admin UI
```

Full chain: `PHP pinba extension` → `UDP:30002` → `Pinba Engine (ha_pinba.so)` inside MySQL →
`ENGINE=PINBA tables in pinba database` → `Pinboard (Symfony admin app)` → developer's browser.

## Core Technical Insights

### 1. The ABI Rule (most critical)
MySQL 8.0 and 8.4 plugins are **not interchangeable**. The entire dual-build strategy
(two Dockerfiles, two CMake presets, two Docker Hub tag tracks) exists because of this.
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

### 5. Docker: libprotobuf Must Be Copied
The MySQL runtime image doesn't have libprotobuf. The Dockerfile explicitly copies it
from the builder stage. Missing this breaks the plugin at load time.
See [[docker-build-strategy]].

### 6. PPA Publication Is Automated
Source packages for Launchpad PPA are now built and uploaded from GitHub Actions,
using the Launchpad signing key and SFTP upload path. See [[github-actions-ppa]].

### 7. Target MySQL Versions Are Monitored
Ubuntu MySQL availability for the PPA target suites is checked by a weekly GitHub Actions
workflow so changes in `noble` or `resolute` can be surfaced before the next release.
See [[github-actions-mysql-version-monitor]].

## Key Configuration Points

| What | Where | Critical Setting |
|------|-------|-----------------|
| PHP extension | `php.ini` | `pinba.enabled = 1` (off by default!) |
| PHP server address | `php.ini` | `pinba.server = "host:port"` |
| Pinba listen port | `my.cnf [mysqld]` | `pinba_port = 30002` |
| Data retention | `my.cnf [mysqld]` | `pinba_stats_history = 900` |
| Docker MySQL series | Dockerfile / tag | must match running MySQL version exactly |

## Project Status

Active modernization fork. Both MySQL 8.0 and 8.4 builds are tested via Docker.
Docker Hub: `xolegator/pinba-engine` with tags `8.0`, `8.4-lts`.
Tag cleanup in progress (removing `*-pinboard*` legacy tags).
Launchpad PPA publication is automated through GitHub Actions for the source package flow.
MySQL version drift across Ubuntu target suites is tracked by automation as well.

## Knowledge Gaps

See [[index]] for the current list of topics not yet covered in this wiki.
