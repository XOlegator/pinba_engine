---
title: "MySQL Plugin API — Installation & Architecture"
type: source
sources:
  - raw/docs/mysql-plugin-api-install.md
related:
  - wiki/concepts/mysql-plugin-abi.md
  - wiki/concepts/mysql-plugin-install.md
  - wiki/concepts/pinba-engine-internals.md
confidence: high
updated: 2026-05-23
---

# MySQL Plugin API — Installation & Architecture

## Key Facts

- `INSTALL PLUGIN` registers in `mysql.plugin` table → persists across restarts
- Plugin must be in `@@global.plugin_dir` (no subdirectories)
- Requires `INSERT` on `mysql.plugin`
- One library can contain multiple plugins

## ABI Incompatibility: 8.0 vs 8.4

**Critical**: MySQL 8.0 and 8.4 plugins are NOT interchangeable.
ABI version is embedded via `MYSQL_HANDLERTON_INTERFACE_VERSION`.
MySQL checks this at `INSTALL PLUGIN` and server start.

This drives the project's entire dual-build strategy:
- Separate Dockerfiles per series
- Separate CMake presets per series
- Separate Docker Hub tags (`8.0`, `8.4-lts`)

## Plugin Descriptor Structure

```c
mysql_declare_plugin(name) {
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &handlerton_struct,
    "NAME",                    // used in INSTALL PLUGIN
    "Author",
    "Description",
    PLUGIN_LICENSE_GPL,
    init_func,
    check_uninstall_func,
    deinit_func,
    version,
    status_vars,
    system_vars,               // MYSQL_SYSVAR_* array
    NULL,
    0
} mysql_declare_plugin_end;
```

## Startup Behaviour

- Normal start: plugins in `mysql.plugin` auto-load
- `--skip-grant-tables`: plugins are NOT loaded (important for recovery scenarios)
- Crash or unclean shutdown: plugin state (Pinba data in RAM) is lost

See: [[mysql-plugin-abi]], [[mysql-plugin-install]]
