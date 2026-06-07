---
title: "Session: Debian PPA Packaging (2026-06-04)"
type: source
sources:
  - raw/sessions/ppa-packaging-session-2026-06-04.md
related:
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/mysql-postinst-patterns.md
  - wiki/concepts/mysql-vendor-headers-minimal.md
  - wiki/concepts/mysql-plugin-install.md
confidence: high
updated: 2026-06-07
---

# Source: Debian PPA Packaging Session — 2026-06-04

A live debugging and implementation session for Debian/PPA packaging of `pinba_engine`.
Went through several fix iterations for postinst/prerm scripts.

## Key facts

- Stage 1.5 completed: vendor headers committed to git (commit `df68221`)
- Stage 2 completed: `dpkg-buildpackage -us -uc -b` succeeds, lintian clean
- Install test passed: fully automatic on `sudo dpkg -i`
- `vendor/mysql-headers-*` optimized: 1317 → 162/175 files, 14 → 2.2 MB per series

## Problems found during testing

| Problem | Root cause | Fix |
|---------|-----------|-----|
| postinst was silent, database not created | `2>/dev/null` suppressed errors | Remove suppression from working commands |
| `INSTALL PLUGIN IF NOT EXISTS` → ERROR 1064 | MySQL 9.0+ syntax, not 8.0 | Check via `information_schema.PLUGINS` |
| `UNINSTALL PLUGIN IF EXISTS` → ERROR 1064 | Same | Same |
| ERROR 3883: Operation not permitted | `debian-sys-maint` lacks SYSTEM_VARIABLES_ADMIN | Use `plugin-load-add` config as the primary path |
| `DROP DATABASE pinba` → Unknown storage engine | Plugin not loaded, tables are ENGINE=PINBA | Load plugin → drop → unload |
| postinst "MySQL not running" with MySQL running | root@localhost protected by password, not auth_socket | Fallback via `/etc/mysql/debian.cnf` |

## Commits

- `df68221` — vendor headers 8.0.46 + 8.4.9
- `db66097` — fix postinst/prerm (debian.cnf, plugin-load-add, correct syntax)
- `d2a19fb` — gitignore vendor/*/builddir/
- `57ed76a` — trim vendor headers (1317 → 162/175 files)

## Related concepts

- [[debian-ppa-packaging]] — full PPA packaging process for a MySQL plugin
- [[mysql-postinst-patterns]] — postinst/prerm patterns and pitfalls for MySQL
- [[mysql-vendor-headers-minimal]] — minimal vendor headers strategy
