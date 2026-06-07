---
title: "PHP Version Monitoring"
type: concept
sources:
  - raw/repos/php-pinba-extension.md
related:
  - wiki/sources/pinba-extension-fork.md
  - wiki/concepts/php-extension-build.md
  - wiki/concepts/php-extension-packaging.md
  - wiki/concepts/github-actions-mysql-version-monitor.md
confidence: high
updated: 2026-06-07
---

# PHP Version Monitoring

## Problem

PHP releases new minor branches on a roughly annual schedule. The CI matrix and packaging
targets must track currently supported PHP branches. Hardcoding versions in `ci.yml` leads
to missed branches and stale support matrices.

## Solution

A weekly scheduled workflow queries `php.net/supported-versions.php`, updates the
canonical JSON file, and opens a PR — a human reviews and merges.

## Workflow: `discover-php-versions.yml`

```
Trigger: weekly cron (Monday 04:17 UTC) + manual workflow_dispatch

Job: discover
  1. Checkout repo
  2. Setup PHP 8.4 (via shivammathur/setup-php@v2)
  3. Run: php .github/scripts/update-php-support-matrix.php
  4. Create PR: branch=ci/discover-php-versions, commit="ci(matrix): refresh supported PHP versions"
```

The PR is created with `peter-evans/create-pull-request@v7`. The branch is auto-deleted
on merge. The PR only appears if `php-versions.json` changed.

## Discovery Script: `update-php-support-matrix.php`

The PHP script:

1. Downloads `https://www.php.net/supported-versions.php` (full HTML page)
2. Parses the "Currently Supported Versions" table with DOMDocument + DOMXPath
3. Filters branches: must match `/^\d+\.\d+$/` and be ≥ 8.2
4. For each branch, reads "Active Support Until" and "Security Support Until" columns
5. Classifies: if active support date is past, notes as "security support only until YYYY-MM-DD";
   otherwise "active support until YYYY-MM-DD"
6. Sorts branch list naturally (8.2, 8.3, 8.4, 8.5, ...)
7. Rewrites `generated_at`, `targets.php` (version list), and `notes` in `php-versions.json`
8. Preserves all other fields in the JSON (`source`, `targets.ubuntu`, etc.)

## Output: `.github/php-versions.json`

```json
{
  "generated_at": "2026-06-07",
  "source": "https://www.php.net/supported-versions.php",
  "targets": {
    "ubuntu": ["24.04", "26.04"],
    "php": ["8.2", "8.3", "8.4", "8.5"]
  },
  "notes": {
    "8.2": "security support only until 2026-12-31",
    "8.3": "security support only until 2027-12-31",
    "8.4": "active support until 2026-12-31",
    "8.5": "active support until 2027-12-31"
  }
}
```

The `notes` field is informational — it does not gate anything automatically.

## Consumers of `php-versions.json`

| Consumer | How it reads the file |
|----------|-----------------------|
| `ci.yml` | `jq -c '.targets.php'` → JSON array → `strategy.matrix` |
| Future packaging workflow | `jq .targets.php` + `jq .targets.ubuntu` → upload matrix |
| `.github/packaging-matrix.json` | Updated manually or by a future automation step |

## Gating Rule

**A discovered PHP branch must not be packaged automatically until CI is green.**

The workflow only updates `php-versions.json` and creates a PR. CI runs against the
updated matrix in that PR. If a new PHP branch (e.g., 8.6) fails CI, the PR is blocked
until the extension is fixed. Only then is the branch confirmed as supported.

This is intentionally human-gated: the packager reviews the PR, verifies CI passed,
and merges. The packaging matrix is updated separately.

## Contrast with MySQL Version Monitor

| Aspect | PHP version monitor | MySQL version monitor |
|--------|---------------------|-----------------------|
| Source | php.net HTML table (DOM parsing) | Ubuntu `apt-cache policy` (shell) |
| Output | `.github/php-versions.json` | `.github/mysql-versions.json` |
| PR auto-merge | No — human reviews | Yes — auto-merge enabled |
| On merge triggers | Nothing (no release cycle) | Full automated release cycle |
| Script language | PHP | Bash |
| Cron schedule | Monday 04:17 UTC | Monday 05:00 UTC |

The MySQL monitor auto-merges because a new MySQL minor version requires an ABI update
and a new plugin release. The PHP monitor does not auto-merge because a new PHP version
is additive only — the existing plugin continues working, and packaging is a separate step.

See [[github-actions-mysql-version-monitor]] for the MySQL side.

See: [[php-extension-build]], [[php-extension-packaging]]
