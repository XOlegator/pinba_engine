---
title: "Knowledge Base Index"
type: index
sources: []
related: []
confidence: high
updated: 2026-06-04
---

# Pinba Engine — Knowledge Base Index

Master catalog. Maintained by LLM agents.

## Concepts (15 articles)

| Article | Key Topic |
|---------|-----------|
| [[pinba-data-flow]] | End-to-end pipeline: PHP → UDP → pool → SQL |
| [[pinba-udp-protocol]] | Protobuf message format, timer parallel arrays, field versions |
| [[pinba-pool-model]] | Cyclic buffer memory model, overflow behaviour, monitoring |
| [[mysql-plugin-abi]] | **Critical**: 8.0 vs 8.4 ABI incompatibility, implications |
| [[mysql-plugin-install]] | INSTALL PLUGIN lifecycle, reinstall procedure |
| [[mysql-plugin-migration-80-to-84]] | Porting checklist: ABI, build requirements, OS paths |
| [[php-pinba-configuration]] | php.ini settings, deployment scenarios, silent failure |
| [[cmake-build-system]] | Presets, MySQL headers sourcing, Ubuntu 24.04 deps |
| [[docker-build-strategy]] | Multi-stage build, libprotobuf copy, filename gotchas |
| [[docker-tag-strategy]] | Docker Hub naming rules, rolling vs pinned tags |
| [[github-actions-docker]] | CI/CD workflow for Docker Hub build+push automation |
| [[pinboard-architecture]] | Pinboard (Symfony 8) stack, aggregation logic, docker compose |
| [[debian-ppa-packaging]] | Debian/PPA packaging: debian/ structure, rules, versioning, Launchpad |
| [[mysql-postinst-patterns]] | postinst/prerm: auth cascade, plugin-load-add, MySQL 8.0 syntax traps |
| [[mysql-vendor-headers-minimal]] | Minimal vendor headers via .d files analysis (1317→162 files) |

## Sources (10 documents)

| Source | Raw File |
|--------|---------|
| [[tony2001-readme]] | `raw/repos/tony2001-pinba-engine-readme.md` |
| [[php-pinba-extension]] (source) | `raw/repos/php-pinba-extension.md` |
| [[pinba-udp-protocol]] (source) | `raw/docs/pinba-udp-protocol-proto.md` |
| [[mysql-plugin-api]] (source) | `raw/docs/mysql-plugin-api-install.md` |
| [[pinba-engine-sysvars-and-tables]] | `raw/docs/pinba-engine-mysql-sysvars.md` + `raw/docs/pinba-default-tables.md` |
| [[fork-build-docker]] | `raw/docs/xolegator-fork-build-docker.md` |
| [[pinboard-architecture]] (raw) | `raw/repos/pinboard-architecture.md` |
| [[mysql-80-to-84-migration]] (raw) | `raw/docs/mysql-80-to-84-migration.md` |
| [[github-actions-docker-workflow]] (raw) | `raw/docs/github-actions-docker-workflow.md` |
| [[ppa-packaging-session-2026-06-04]] | `raw/sessions/ppa-packaging-session-2026-06-04.md` |

## Raw Documents

```
raw/repos/
  tony2001-pinba-engine-readme.md
  tony2001-pinba-engine-changelog.md
  php-pinba-extension.md
  pinboard-architecture.md

raw/docs/
  pinba-udp-protocol-proto.md
  pinba-engine-mysql-sysvars.md
  pinba-default-tables.md
  mysql-plugin-api-install.md
  xolegator-fork-build-docker.md
  mysql-80-to-84-migration.md
  github-actions-docker-workflow.md
```

## Knowledge Gaps (candidates for next ingest)

- PHP site integration examples (real php.ini configuration with real server names)
- Production deployment: docker-compose with volumes, backups, log rotation
- Pinboard UI: controller/route structure, what each page shows
- GitHub Actions PPA build workflow (ppa-build.yml) — ЭТАП 5 плана
- MySQL version monitor workflow — ЭТАП 6 плана
