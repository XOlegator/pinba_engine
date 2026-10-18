---
title: "MySQL Plugin Installation and Lifecycle"
type: concept
sources:
  - raw/docs/mysql-plugin-api-install.md
  - raw/docs/pinba-engine-mysql-sysvars.md
related:
  - wiki/concepts/mysql-plugin-abi.md
  - wiki/concepts/request-structure-validation.md
confidence: high
updated: 2026-10-18
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
                                                    ├─ initialize locks, pools and tag registry
                                                    ├─ bind UDP socket on pinba_port
                                                    └─ start workers and wait for readiness

INSTALL PLUGIN → same init sequence, plugin now persists

UNINSTALL PLUGIN → call pinba_engine_check_uninstall() [can reject if tables in use]
                   → call pinba_engine_shutdown()
                     ├─ stop threads
                     └─ free pools (all data lost)

Server stop → same as UNINSTALL but without check_uninstall
```

## Checking Plugin Status

```sql
SHOW PLUGINS;
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

## Partial initialization and cooperative shutdown

Initialization tracks each initialized lock, pool and started thread. A failure
uses the same cleanup path as normal shutdown, joining only threads actually
started and destroying only initialized resources. The tag registry exists
before any collector starts. Worker readiness is acknowledged before init can
return success; early worker allocation failures fail initialization as well.
Thread-pool creation catches allocation/thread-construction exceptions and joins
the already created workers. Decode allocation failures return an error rather
than unwinding through a held collector lock.

Shutdown uses an atomic flag and a shared level-triggered eventfd registered in
each collector's epoll instance. This wakes every listener without cancellation
or idle polling. Data/stats waits also observe the descriptor, so even a long
gathering period does not delay shutdown. Collectors own epoll/buffers with RAII;
the collector thread array and all nine rwlocks are released.

RENAME moves the table binding and its entry in the report's alias set; the
report's canonical key stays unchanged. DROP after RENAME releases the correct
owner. ALTER COMMENT remains unsupported: rejection preserves the old report.
Condition parsing no longer subtracts a null comma pointer, and report-kind
classification is initialized for ordinary comments as well as histogram views.

`scripts/lifecycle_test.py` exercises concurrent SQL/UDP and RENAME/DROP, rejected
ALTER, query cancellation, repeated failed bind/INSTALL and successful retry.
It requires an isolated server with history <= 10. `--server-pid` also checks
that fd/thread counts return to baseline.

For controlled startup failures in a disposable **test build**, configure
`PINBA_ENABLE_TEST_FAILURES=ON`, set `PINBA_INIT_FAILURE_FILE` in the server's
environment, and pass the same file to `--failure-file`. Ten failure stages cover
pool/thread allocation and asynchronous data/stats buffer initialization. The
normal default is OFF and compiles the failure controls out.

Timer-pool growth failure no longer publishes an out-of-bounds reservation.
Timer tag-array allocation failures retain old buffers and initialize reserved
timer slots; an optional tag lookup-cache allocation can fall back to lookup.
These corrections do not constitute a guarantee that every legacy allocation
site in every report algorithm tolerates exhaustion.

See: [[mysql-plugin-abi]], [[request-structure-validation]]
