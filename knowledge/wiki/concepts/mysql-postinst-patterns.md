---
title: "MySQL Plugin postinst/prerm Patterns and Pitfalls"
type: concept
sources:
  - raw/sessions/ppa-packaging-session-2026-06-04.md
related:
  - wiki/concepts/debian-ppa-packaging.md
  - wiki/concepts/mysql-plugin-install.md
confidence: high
updated: 2026-07-16
---

# MySQL Plugin postinst/prerm — Patterns and Pitfalls

Reference for writing `postinst` and `prerm` scripts for Debian/Ubuntu packages that
install MySQL storage engine plugins. All pitfalls were discovered during real debugging
on Ubuntu 24.04 + MySQL 8.0.46.

---

## Authentication in postinst (Ubuntu)

`postinst` runs as `root` via `sudo dpkg -i`. It needs to connect to MySQL.

### Authentication cascade

```sh
DEBIAN_CNF="/etc/mysql/debian.cnf"
MYSQL_SOCKET="/var/run/mysqld/mysqld.sock"

mysql_available() {
    # 1. debian-sys-maint via /etc/mysql/debian.cnf (600 root:root)
    if [ -r "$DEBIAN_CNF" ] && \
       mysql --defaults-file="$DEBIAN_CNF" -e "SELECT 1" >/dev/null 2>&1; then
        return 0
    fi
    # 2. root via auth_socket (works if root@localhost has no password)
    mysql --no-defaults -u root -S "$MYSQL_SOCKET" -e "SELECT 1" >/dev/null 2>&1
}

mysql_exec() {
    if [ -r "$DEBIAN_CNF" ]; then
        mysql --defaults-file="$DEBIAN_CNF" "$@"
    else
        mysql --no-defaults -u root -S "$MYSQL_SOCKET" "$@"
    fi
}
```

### /etc/mysql/debian.cnf

- Present on all Ubuntu MySQL installations (`mysql-server-8.0`, `mysql-community-server`)
- Contains the `debian-sys-maint` login and password
- File permissions: `600 root:root` → readable only from `postinst` (which runs as root)
- `debian-sys-maint` has `ALL PRIVILEGES ON *.*` — SELECT, CREATE DATABASE, INSERT, etc.
- **However:** on Ubuntu 24.04 MySQL 8.0, `debian-sys-maint` does NOT have `SYSTEM_VARIABLES_ADMIN`
  (required for `INSTALL PLUGIN`)

### What debian-sys-maint can and cannot do

| Operation | Works |
|-----------|-------|
| SELECT, SHOW | ✓ |
| CREATE DATABASE | ✓ |
| source .sql (CREATE TABLE ENGINE=PINBA) | ✓ (if plugin is loaded) |
| INSTALL PLUGIN | ✗ (no SYSTEM_VARIABLES_ADMIN) |
| UNINSTALL PLUGIN | ✗ |

---

## Plugin installation: plugin-load-add vs INSTALL PLUGIN

### INSTALL PLUGIN

- Registers the plugin in the `mysql.plugin` table (persistent across restarts)
- Activates immediately
- **Requires** `SYSTEM_VARIABLES_ADMIN` or `SUPER`
- Does not work on Ubuntu 24.04 via `debian-sys-maint`

### plugin-load-add (recommended as primary method)

```sh
cat > /etc/mysql/conf.d/pinba-engine.cnf << 'EOF'
[mysqld]
plugin-load-add = ha_pinba.so
EOF
```

- Requires no MySQL privileges — just create a file
- Plugin loads on every MySQL startup
- Does **not** activate immediately — requires a MySQL restart
- Compatible with `INSTALL PLUGIN` (if both are active, the duplication is harmless)

### Correct postinst strategy

```sh
# 1. Always write plugin-load-add (no privileges required, restart-safe)
mkdir -p /etc/mysql/conf.d
printf '[mysqld]\nplugin-load-add = ha_pinba.so\n' > /etc/mysql/conf.d/pinba-engine.cnf

# 2. Attempt INSTALL PLUGIN for immediate activation (may not succeed)
if ! mysql_exec -e "INSTALL PLUGIN pinba SONAME 'ha_pinba.so';" >/dev/null 2>&1; then
    echo "plugin will load on next MySQL restart"
fi
```

---

## Syntax pitfalls: MySQL 8.0 vs 9.0

| Syntax | MySQL 8.0 | MySQL 9.0+ |
|--------|-----------|------------|
| `INSTALL PLUGIN IF NOT EXISTS name SONAME '...'` | ❌ ERROR 1064 | ✓ |
| `UNINSTALL PLUGIN IF EXISTS name` | ❌ ERROR 1064 | ✓ |

**Correct approach for MySQL 8.0** — check via information_schema:

```sh
# Check if plugin is active
PLUGIN_ACTIVE=$(mysql_exec --skip-column-names \
    -e "SELECT COUNT(*) FROM information_schema.PLUGINS \
        WHERE PLUGIN_NAME='pinba' AND PLUGIN_STATUS='ACTIVE';" \
    2>/dev/null || echo "0")

# Install only if not active
if [ "${PLUGIN_ACTIVE:-0}" = "0" ]; then
    mysql_exec -e "INSTALL PLUGIN pinba SONAME 'ha_pinba.so';"
fi

# Unload only if active
if [ "${PLUGIN_ACTIVE:-0}" != "0" ]; then
    mysql_exec -e "UNINSTALL PLUGIN pinba;" || true
fi
```

---

## Creating the pinba database and tables

```sh
# Check if the database exists (safe even if the plugin is not loaded)
DB_EXISTS=$(mysql_exec --skip-column-names \
    -e "SELECT COUNT(*) FROM information_schema.SCHEMATA \
        WHERE SCHEMA_NAME='pinba';" 2>/dev/null || echo "0")

if [ "${DB_EXISTS:-0}" = "0" ]; then
    mysql_exec -e "CREATE DATABASE pinba CHARACTER SET latin1;"
    # Import tables — only if the plugin is ACTIVE right now
    # (CREATE TABLE ENGINE=PINBA requires the plugin to be loaded)
    if plugin_active; then
        mysql_exec pinba < /usr/share/pinba_engine/default_tables.sql
    fi
fi
```

**Important:** `CREATE TABLE ... ENGINE=PINBA` fails if the plugin is not loaded.
If `INSTALL PLUGIN` did not succeed (insufficient privileges), the import cannot run;
the user must be instructed to do it after the next MySQL restart.

---

## Schema upgrades during package updates

When a package ships a DDL fix for already initialized PINBA tables, `postinst`
can apply it automatically if:

- MySQL/MariaDB is reachable during package configuration
- the `pinba` schema already exists
- the Pinba plugin is active in the running server

Pattern:

```sh
if plugin_active && schema_exists; then
    mysql_exec pinba < /usr/share/pinba_engine/upgrade_hostname_64.sql
else
    echo "run the upgrade script manually after restart"
fi
```

This keeps `apt upgrade` mostly automatic while preserving a safe fallback for
cases where the plugin is not active yet or the server is offline during package
configuration.

---

## DROP DATABASE with ENGINE=PINBA tables

When removing the package (`dpkg -r`), `ha_pinba.so` is deleted from disk.
If the plugin was loaded via `INSTALL PLUGIN`, the record remains in `mysql.plugin`.

**Pitfall:** attempting `DROP DATABASE pinba` without the plugin loaded:
```
ERROR 1286 (42000): Unknown storage engine 'PINBA'
```
MySQL calls the engine handler even for DROP — if the plugin is missing, it fails.

**Correct manual cleanup order:**
```sql
-- 1. Load the plugin first
INSTALL PLUGIN pinba SONAME 'ha_pinba.so';
-- 2. Now the database can be dropped
DROP DATABASE pinba;
-- 3. Unload the plugin
UNINSTALL PLUGIN pinba;
```

---

## prerm: removing config and unloading the plugin

```sh
case "$1" in
    remove|purge)
        # Remove the plugin-load-add config so the plugin does not load after package removal
        rm -f /etc/mysql/conf.d/pinba-engine.cnf

        # Unload the plugin if it is currently active
        if mysql_available 2>/dev/null; then
            ACTIVE=$(mysql_exec --skip-column-names \
                -e "SELECT COUNT(*) FROM information_schema.PLUGINS \
                    WHERE PLUGIN_NAME='pinba' AND PLUGIN_STATUS='ACTIVE';" \
                2>/dev/null || echo "0")
            if [ "${ACTIVE:-0}" != "0" ]; then
                mysql_exec -e "UNINSTALL PLUGIN pinba;" 2>&1 || true
            fi
        fi
        ;;
esac
```

---

## Output UX: [OK] / [--] / [!!]

Recommended approach — mark the status of each step explicitly:
- `[OK]` — completed successfully
- `[--]` — skipped (expected, not an error)
- `[!!]` — warning (partially completed)

Suppress raw MySQL errors (`>/dev/null 2>&1`) on operations that may legitimately fail
(e.g., `INSTALL PLUGIN`), and emit only your own status messages.

End with a summary "Action required" block if the user needs to do anything manually.
