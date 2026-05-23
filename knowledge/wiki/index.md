---
title: "Knowledge Base Index"
type: index
sources: []
related: []
confidence: high
updated: 2026-05-23
---

# Pinba Engine — Knowledge Base Index

Master catalog. Maintained by LLM agents.

## Concepts (8 articles)

| Article | Key Topic |
|---------|-----------|
| [[pinba-data-flow]] | End-to-end pipeline: PHP → UDP → pool → SQL |
| [[pinba-udp-protocol]] | Protobuf message format, timer parallel arrays, field versions |
| [[pinba-pool-model]] | Cyclic buffer memory model, overflow behaviour, monitoring |
| [[mysql-plugin-abi]] | **Critical**: 8.0 vs 8.4 ABI incompatibility, implications |
| [[mysql-plugin-install]] | INSTALL PLUGIN lifecycle, reinstall procedure |
| [[php-pinba-configuration]] | php.ini settings, deployment scenarios, silent failure |
| [[cmake-build-system]] | Presets, MySQL headers sourcing, Ubuntu 24.04 deps |
| [[docker-build-strategy]] | Multi-stage build, libprotobuf copy, filename gotchas |
| [[docker-tag-strategy]] | Docker Hub naming rules, rolling vs pinned tags |

## Sources (6 documents)

| Source | Raw File |
|--------|---------|
| [[tony2001-readme]] | `raw/repos/tony2001-pinba-engine-readme.md` |
| [[php-pinba-extension]] (source) | `raw/repos/php-pinba-extension.md` |
| [[pinba-udp-protocol]] (source) | `raw/docs/pinba-udp-protocol-proto.md` |
| [[mysql-plugin-api]] (source) | `raw/docs/mysql-plugin-api-install.md` |
| [[pinba-engine-sysvars-and-tables]] | `raw/docs/pinba-engine-mysql-sysvars.md` + `raw/docs/pinba-default-tables.md` |
| [[fork-build-docker]] | `raw/docs/xolegator-fork-build-docker.md` |

## Raw Documents

```
raw/repos/
  tony2001-pinba-engine-readme.md
  tony2001-pinba-engine-changelog.md
  php-pinba-extension.md

raw/docs/
  pinba-udp-protocol-proto.md
  pinba-engine-mysql-sysvars.md
  pinba-default-tables.md
  mysql-plugin-api-install.md
  xolegator-fork-build-docker.md
```

## Knowledge Gaps (candidates for next ingest)

- MySQL 8.0 → 8.4 specific API changes (what exactly broke, what changed)
- Pinboard architecture (Symfony, aggregation logic, Doctrine queries)
- Docker Compose full stack for production deployment
- PHP site integration examples (real php.ini configuration)
- GitHub Actions CI workflow for Docker build+push automation
