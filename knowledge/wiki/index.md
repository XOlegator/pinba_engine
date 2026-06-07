---
title: "Knowledge Base Index"
type: index
sources: []
related: []
confidence: high
updated: 2026-06-07
---

# Pinba Engine — Knowledge Base Index

Master catalog. Maintained by LLM agents.

## Concepts (22 articles)

| Article | Key Topic |
|---------|-----------|
| [[pinba-data-flow]] | End-to-end pipeline: PHP → UDP → pool → SQL |
| [[pinba-udp-protocol]] | Protobuf message format, timer parallel arrays, field versions |
| [[pinba-pool-model]] | Cyclic buffer memory model, overflow behaviour, monitoring |
| [[mysql-plugin-abi]] | **Critical**: 8.0 vs 8.4 ABI incompatibility, implications |
| [[mysql-plugin-install]] | INSTALL PLUGIN lifecycle, reinstall procedure |
| [[mysql-plugin-migration-80-to-84]] | Porting checklist: ABI, build requirements, OS paths |
| [[php-pinba-configuration]] | php.ini settings, deployment scenarios, silent failure |
| [[php-extension-api]] | Complete PHP function reference: timer lifecycle, request tags, overrides |
| [[php-extension-build]] | phpize build system, PHPT tests, multi-PHP CI matrix |
| [[php-extension-packaging]] | Debian packaging for PHP extensions: php-pinba source, php{X.Y}-pinba binaries |
| [[php-version-monitoring]] | Automated PHP version discovery: weekly cron, php.net parsing, PR gate |
| [[cmake-build-system]] | Presets, MySQL headers sourcing, Ubuntu 24.04 deps |
| [[docker-build-strategy]] | Multi-stage build, libprotobuf copy, filename gotchas |
| [[docker-tag-strategy]] | Docker Hub naming rules, rolling vs pinned tags |
| [[github-actions-docker]] | CI/CD workflow for Docker Hub build+push automation |
| [[github-actions-ppa]] | GitHub Actions workflow: source package build, signing, multi-series, FTP upload with passive mode and retry |
| [[github-actions-mysql-version-monitor]] | Weekly MySQL version monitor: auto-PR with vendor header update and full release cycle trigger |
| [[pinboard-architecture]] | Pinboard (Symfony 8) stack, aggregation logic, docker compose |
| [[debian-ppa-packaging]] | Debian/PPA packaging: debian/ structure, rules, versioning, Build-Depends, rapidjson |
| [[mysql-postinst-patterns]] | postinst/prerm: auth cascade, plugin-load-add, MySQL 8.0 syntax pitfalls |
| [[mysql-vendor-headers-minimal]] | Minimal vendor headers via .d file analysis (1317→162 files) |
| [[launchpad-ppa-workflow]] | dput configuration, signing, versioning, GPGKeyTemporarilyNotFoundError |

## Sources (12 documents)

| Source | Raw File |
|--------|---------|
| [[tony2001-readme]] | `raw/repos/tony2001-pinba-engine-readme.md` |
| [[php-pinba-extension]] (source) | `raw/repos/php-pinba-extension.md` |
| [[pinba-extension-fork]] (source) | XOlegator/pinba_extension — repo structure, changes, tests |
| [[pinba-udp-protocol]] (source) | `raw/docs/pinba-udp-protocol-proto.md` |
| [[mysql-plugin-api]] (source) | `raw/docs/mysql-plugin-api-install.md` |
| [[pinba-engine-sysvars-and-tables]] | `raw/docs/pinba-engine-mysql-sysvars.md` + `raw/docs/pinba-default-tables.md` |
| [[fork-build-docker]] | `raw/docs/xolegator-fork-build-docker.md` |
| [[pinboard-architecture]] (raw) | `raw/repos/pinboard-architecture.md` |
| [[mysql-80-to-84-migration]] (raw) | `raw/docs/mysql-80-to-84-migration.md` |
| [[github-actions-docker-workflow]] (raw) | `raw/docs/github-actions-docker-workflow.md` |
| [[ppa-packaging-session-2026-06-04]] | `raw/sessions/ppa-packaging-session-2026-06-04.md` |
| [[ppa-upload-session-2026-06-06]] | `raw/sessions/ppa-upload-session-2026-06-06.md` |

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
- `debian/` skeleton for php-pinba source package (once created in pinba_extension repo)
