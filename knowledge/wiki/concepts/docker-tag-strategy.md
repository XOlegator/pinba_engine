---
title: "Docker Hub Tag Strategy"
type: concept
sources:
  - raw/docs/xolegator-fork-build-docker.md
related:
  - wiki/concepts/docker-build-strategy.md
  - wiki/concepts/mysql-plugin-abi.md
confidence: high
updated: 2026-05-23
---

# Docker Hub Tag Strategy

Repository: `xolegator/pinba-engine`

## Naming Convention

### Rolling channel tags (updated on every release)

| Tag | MySQL series | Use case |
|-----|-------------|----------|
| `8.0` | MySQL 8.0.x | Default for most users |
| `8.4-lts` | MySQL 8.4 LTS | Users on MySQL 8.4 LTS |
| `latest` | alias → `8.0` | Convenience |

### Pinned immutable tags (for reproducible deployments)

Format: `{mysql-series}-v{pinba-version}`

| Example | Meaning |
|---------|---------|
| `8.0-v1.2.3` | Pinba Engine v1.2.3 for MySQL 8.0 |
| `8.4-lts-v1.2.3` | Pinba Engine v1.2.3 for MySQL 8.4 LTS |

`{pinba-version}` = SemVer from GitHub Release (e.g., `v1.2.3` → `1.2.3`).

## Why Not MySQL Patch Version in Tag?

Tags like `8.0.46` conflate the MySQL version used at build time with the MySQL version the user runs.
The plugin is compatible with all MySQL 8.0.x patch versions (ABI is stable within a minor series).

## Build and Push Commands

```bash
# MySQL 8.0
docker build -t xolegator/pinba-engine:8.0 \
             -t xolegator/pinba-engine:latest \
             -t xolegator/pinba-engine:8.0-v1.2.3 \
             -f Dockerfile.mysql80 .

# MySQL 8.4 LTS
docker build -t xolegator/pinba-engine:8.4-lts \
             -t xolegator/pinba-engine:8.4-lts-v1.2.3 \
             -f Dockerfile.mysql84 .

docker push xolegator/pinba-engine:8.0
docker push xolegator/pinba-engine:latest
docker push xolegator/pinba-engine:8.0-v1.2.3
docker push xolegator/pinba-engine:8.4-lts
docker push xolegator/pinba-engine:8.4-lts-v1.2.3
```

## Future: GitHub Actions CI

On release tag `v*`:
1. Build both images with pinba version embedded
2. Push rolling tags (`8.0`, `8.4-lts`, `latest`)
3. Push pinned tags (`8.0-vX.Y.Z`, `8.4-lts-vX.Y.Z`)

See: [[docker-build-strategy]], [[mysql-plugin-abi]]
