---
title: "Docker Hub Tag Strategy"
type: concept
sources:
  - raw/docs/xolegator-fork-build-docker.md
  - raw/docs/github-actions-docker-workflow.md
related:
  - wiki/concepts/docker-build-strategy.md
  - wiki/concepts/github-actions-docker.md
  - wiki/concepts/mysql-plugin-abi.md
  - wiki/concepts/pinboard-architecture.md
confidence: high
updated: 2026-07-12
---

# Docker Hub Tag Strategy

Repository: `xolegator/pinba-engine`

> **Scheme change (2026-07, PR #87):** the old `8.4-lts` / `8.4-lts-vX.Y.Z` tags are
> **dead** — frozen at v2.0.0 (2026-05-23) and never updated again. Anything still
> referencing `8.4-lts*` is silently pinned to a stale engine build. The current
> scheme below has no `-lts` suffix and adds MariaDB channels.

## Naming Convention

### Channel tags (rolling — move with every Pinba release)

| Tag | Database | Architectures |
|-----|----------|---------------|
| `8.0` | MySQL 8.0.x | `linux/amd64` only |
| `8.4` | MySQL 8.4 LTS | `linux/amd64`, `linux/arm64` |
| `mariadb-10.11` | MariaDB 10.11 LTS | `linux/amd64`, `linux/arm64` |
| `mariadb-11.8` | MariaDB 11.8 LTS | `linux/amd64`, `linux/arm64` |
| `latest` | alias → `8.4` (MySQL 8.4 LTS) | same as `8.4` |

The `8.0` channel is amd64-only because the upstream `mysql:8.0` Debian
(`-bookworm`) base image is not published for arm64. Note `latest` now aliases
the **8.4** channel (the old scheme aliased 8.0).

### Version-suffixed tags (per Pinba release)

Format: `{channel}-v{pinba-version}`, e.g. `8.4-v2.11.3`, `mariadb-11.8-v2.11.3`.
`{pinba-version}` = SemVer from the GitHub Release (`v2.11.3` → `2.11.3`); on
non-release rebuilds the version is read from `.release-please-manifest.json`.

## All tags are mutable — pin by digest

**No published tag is immutable, including version-suffixed ones:**

- Channel tags move with every Pinba release.
- Version tags are **re-pushed** when the base MySQL/MariaDB image gets a patch
  bump: the MySQL-version-monitor automation updates the Dockerfile base and the
  Docker workflow rebuilds, so `8.4-v2.11.3` can point to a different image
  between Pinba releases.

For reproducible deployments pin by **digest**
(`xolegator/pinba-engine@sha256:…`). `docs/docker.md` in the repo carries a
digest table per channel that CI (`update-docs-digests` job) refreshes
automatically after every push. Resolve manually with
`docker buildx imagetools inspect xolegator/pinba-engine:<tag>`.

## Why Not MySQL Patch Version in Tag?

Tags like `8.0.46` conflate the MySQL version used at build time with the MySQL
version the user runs. The plugin is compatible with all MySQL 8.0.x patch
versions (ABI is stable within a minor series) — see [[mysql-plugin-abi]].

## CI Publishing (`.github/workflows/docker.yml`)

Build and push is automated (no manual `docker push`); the old
`docker-push.yml` name is gone. Triggers:

1. **Release published** — full version + channel tags for all four channels.
2. **Push to `master`** touching `Dockerfile.*`, `docker/**`,
   `docker-entrypoint-initdb.d/**` or the workflow itself (e.g. a base-image
   bump) — re-publishes with the version from `.release-please-manifest.json`.
3. **`workflow_dispatch`** with an optional `release-tag` input for re-publishing
   a past release.

Pipeline per channel (4-entry matrix): build amd64 image locally → **smoke test
gate** (container must reach `plugin_status = ACTIVE` for the pinba plugin
before anything is pushed — an INSTALL PLUGIN failure is not catchable at build
time) → multi-arch build & push (arm64 via QEMU) → `update-docs-digests` job
refreshes the digest table in `docs/docker.md`. Runs are serialized via a
`docker-publish` concurrency group so channel tags cannot land out of order.

## Downstream usage (Pinboard)

Pinboard (see [[pinboard-architecture]]) defaults to the **rolling `8.4`
channel** (overridable via `PINBA_ENGINE_TAG`, e.g. `8.4-v2.11.3` to pin) so
that new pinba_engine releases require no reconfiguration on the Pinboard side.

See: [[docker-build-strategy]], [[github-actions-docker]], [[mysql-plugin-abi]]
