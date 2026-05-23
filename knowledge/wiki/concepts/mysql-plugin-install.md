---
title: "MySQL Plugin Installation and Lifecycle"
type: concept
sources:
  - raw/docs/mysql-plugin-api-install.md
  - raw/docs/pinba-engine-mysql-sysvars.md
related:
  - wiki/concepts/mysql-plugin-abi.md
  - wiki/concepts/pinba-engine-internals.md
confidence: high
updated: 2026-05-23
---

# MySQL Plugin Installation and Lifecycle

## Installation

```sql
INSTALL PLUGIN pinba SONAME 'ha_pinba.so';
-- or in Docker: SONAME 'libpinba_engine.so'
```

- Plugin file must be in `@@global.plugin_dir` (exact directory, no subdirs)
- Registered in `mysql.plugin` table → **auto-loaded on every server start**
- One-time operation; MySQL reads `my.cnf` at install time (picks up `pinba_*` variables)

## Getting the Plugin Directory

```bash
mysql -uroot -p --batch --skip-column-names -e "SELECT @@global.plugin_dir;"
```

## Lifecycle

```
Server start → mysql.plugin table → load plugin → call pinba_engine_init()
                                                    │
                                                    ├─ bind UDP socket on pinba_port
                                                    ├─ allocate memory pools
                                                    └─ start listener + stats threads

INSTALL PLUGIN → same init sequence, plugin now persists

UNINSTALL PLUGIN → call pinba_engine_check_uninstall() [can reject if tables in use]
                   → call pinba_engine_shutdown()
                     ├─ stop threads
                     └─ free pools (all data lost)

Server stop → same as UNINSTALL but without check_uninstall
```

## Checking Plugin Status

```sql
SHOW PLUGINS LIKE 'pinba';
SELECT PLUGIN_NAME, PLUGIN_STATUS FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME = 'PINBA';
-- PLUGIN_STATUS = 'ACTIVE' means running correctly
```

## Reinstalling After Recompile (local)

```bash
mysql -uroot -p -e "UNINSTALL PLUGIN pinba;"
sudo cp build/ha_pinba.so "$(mysql ... SELECT @@global.plugin_dir ...)/"
mysql -uroot -p -e "INSTALL PLUGIN pinba SONAME 'ha_pinba.so';"
```

If `UNINSTALL` fails (tables in use): stop all sessions using pinba tables, or restart MySQL
after replacing the `.so` file (restart re-reads from `mysql.plugin` automatically).

## Important: --skip-grant-tables

When MySQL starts with `--skip-grant-tables` (emergency mode), plugins from `mysql.plugin`
are NOT loaded. Pinba will be absent until normal restart.

## Docker Initialization

In Docker, `docker-entrypoint-initdb.d/` scripts run on first container start only.
`01-init-pinba.sh` handles: INSTALL PLUGIN + CREATE DATABASE + default_tables.sql.
On subsequent starts, the plugin loads automatically from `mysql.plugin`.

See: [[mysql-plugin-abi]], [[pinba-engine-internals]]
