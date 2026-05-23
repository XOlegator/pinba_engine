---
title: "MySQL Storage Engine Plugin ABI"
type: concept
sources:
  - raw/docs/mysql-plugin-api-install.md
  - raw/docs/xolegator-fork-build-docker.md
related:
  - wiki/concepts/mysql-plugin-install.md
  - wiki/concepts/cmake-build-system.md
  - wiki/concepts/docker-build-strategy.md
confidence: high
updated: 2026-05-23
---

# MySQL Storage Engine Plugin ABI

## The Core Constraint

**A plugin compiled against MySQL 8.0 headers will NOT load in MySQL 8.4 and vice versa.**

This is the single most important constraint in the entire project. Every architectural decision
about Docker images, build presets, CI pipelines flows from this fact.

## Why ABI Incompatibility Exists

MySQL 8.0→8.4 changed internal C++ structures (`handlerton`, `handler`, `TABLE_SHARE`, etc.)
in ways that affect the plugin binary interface. The `MYSQL_HANDLERTON_INTERFACE_VERSION`
constant changes between MySQL versions. MySQL checks this at plugin load time.

If versions mismatch, MySQL refuses to load the plugin with an error like:
```
ERROR 1126 (HY000): Can't open shared library 'libpinba_engine.so'
  (errno: 0 API version for STORAGE ENGINE plugin is too different)
```

**This applies to minor versions too.** MySQL 8.0.45 and 8.0.46 have different
`MYSQL_HANDLERTON_INTERFACE_VERSION` values, causing the same error if the plugin
was compiled against 8.0.45 source headers but loaded into 8.0.46 at runtime.
The fix is to keep the source version in `CMakeLists.txt` in sync with the runtime
image tag in the Dockerfile.

## Implications for This Project

| Concern | Solution |
|---------|---------|
| Build | Separate CMake presets: `release` (8.0), `release-download-mysql84-source` (8.4) |
| Docker | Separate Dockerfiles: `Dockerfile.mysql80`, `Dockerfile.mysql84` |
| Docker Hub | Separate tags: `8.0`, `8.4-lts` |
| CI | Must build and test both series independently |

## Compile-Time Detection

The build system uses `PINBA_MYSQL_SERIES` CMake variable to switch header paths and
conditional compilation blocks. Key places where 8.0/8.4 diverge are wrapped in:
```cpp
#if MYSQL_VERSION_ID >= 80400
  // MySQL 8.4+ code path
#else
  // MySQL 8.0 code path
#endif
```

## ABI-Safe Practices

- **Never mix** plugin and MySQL versions
- In Docker: builder and runtime base OS must match (both `debian:bookworm`) to ensure
  glibc/glibcxx ABI compatibility
- `libprotobuf.so` must be from the same build environment as the plugin (hence explicit COPY
  in Dockerfile rather than relying on runtime image's system libprotobuf)

See: [[cmake-build-system]], [[docker-build-strategy]], [[mysql-plugin-install]]
