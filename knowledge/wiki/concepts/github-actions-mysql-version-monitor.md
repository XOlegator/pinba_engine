---
title: "GitHub Actions: MySQL Version Monitor"
type: concept
sources: []
related:
  - wiki/concepts/github-actions-ppa.md
  - wiki/concepts/debian-ppa-packaging.md
confidence: high
updated: 2026-06-07
---

# GitHub Actions: MySQL Version Monitor

Workflow for tracking which MySQL series are currently available from Ubuntu repositories
for the PPA target releases and automatically kicking off the full release cycle when
versions change.

## Files

- `.github/mysql-versions.json` — tracked known-good version map (also stores `debian_revision` per suite)
- `.github/scripts/check-mysql-versions.sh` — apt-based detector for one Ubuntu suite
- `.github/workflows/mysql-version-monitor.yml` — weekly/manual monitor workflow

## Tracked suite matrix

Defined in `.github/mysql-versions.json`. Example current state:

```json
{
  "noble":    { "ubuntu_version": "24.04", "mysql80": "8.0.46", "mysql84": null,    "debian_revision": 1 },
  "resolute": { "ubuntu_version": "26.04", "mysql80": null,     "mysql84": "8.4.9", "debian_revision": 1 }
}
```

A `null` value for a MySQL series means that series is not expected from the standard Ubuntu
repositories for that suite.

The `debian_revision` field is incremented automatically by the monitor whenever a version
change is detected, ensuring each new PPA upload gets a fresh revision number.

## What the workflow does

1. Detects current `libmysqlclient-dev` versions available in `ubuntu:24.04` and `ubuntu:26.04`
   by running `apt-cache madison` inside a Docker container.
2. Compares detected versions against `.github/mysql-versions.json`.
3. If any version changed:
   - Re-extracts the minimal MySQL vendor headers for the affected series using
     `tools/extract-mysql-headers.sh`.
   - Updates `.github/mysql-versions.json` (new versions + incremented `debian_revision`).
   - Updates `Dockerfile.mysql80` / `Dockerfile.mysql84` `MYSQL_IMAGE` ARG.
   - Opens a PR on `chore/mysql-version-update` branch via `peter-evans/create-pull-request`.
   - Enables auto-merge (squash) on the PR so it lands without manual intervention
     if branch protection checks pass.
4. If no versions changed: exits cleanly with a log message.

## Downstream automation triggered by the PR merge

When the auto-merge PR lands on `master`:
- `ppa-build` workflow triggers (path filter on `.github/mysql-versions.json`) →
  rebuilds source packages and uploads to Launchpad PPA.
- `docker` workflow triggers (path filter on `Dockerfile.*`) →
  rebuilds and pushes Docker images to Docker Hub.

The result is a fully automated release cycle: MySQL patch update detected on Monday →
PR opens → auto-merges → PPA and Docker Hub updated, all without manual intervention.

## Why auto-merge instead of opening issues

Earlier versions of this workflow opened a GitHub issue instead of a PR. This was changed
because a MySQL patch version bump has a well-defined set of mechanical changes (header
re-extraction, JSON update, Dockerfile pin) that can be performed and verified automatically.
Opening an issue requires manual follow-up; creating an auto-merge PR closes the loop without
human bookkeeping overhead while still giving full visibility via the PR diff and CI results.

## Manual (dry-run) mode

`workflow_dispatch` accepts a `dry_run` boolean input. When true, the workflow detects and
reports version changes but does not modify any files or open a PR.
