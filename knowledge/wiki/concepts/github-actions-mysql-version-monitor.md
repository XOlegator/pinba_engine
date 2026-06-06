---
title: "GitHub Actions: MySQL Version Monitor"
type: concept
sources: []
related:
  - wiki/concepts/github-actions-ppa.md
  - wiki/concepts/debian-ppa-packaging.md
confidence: medium
updated: 2026-06-06
---

# GitHub Actions: MySQL Version Monitor

Workflow for tracking which MySQL series are currently available from Ubuntu repositories
for the PPA target releases.

## Files

- `.github/mysql-versions.json` — tracked known-good version map
- `.github/scripts/check-mysql-versions.sh` — apt-based detector for one Ubuntu suite
- `.github/workflows/mysql-version-monitor.yml` — weekly/manual monitor workflow

## Current tracked matrix

- `noble` → MySQL 8.0.46
- `resolute` → MySQL 8.4.9

`null` in `.github/mysql-versions.json` means that a given series is not expected from
the standard Ubuntu repositories for that suite.

## Why this exists

Launchpad uploads are versioned. A new MySQL patch version is not just a rebuild:
it normally requires updating vendored headers, adjusting the workflow matrix if the
supported series changed, and bumping the Debian revision before a fresh PPA upload.

The monitor therefore opens an issue instead of automatically uploading.
