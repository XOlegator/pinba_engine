---
title: "MySQL Plugin Migration: 8.0 → 8.4"
type: concept
sources:
  - raw/docs/mysql-80-to-84-migration.md
related:
  - wiki/concepts/mysql-plugin-abi.md
  - wiki/concepts/docker-build-strategy.md
  - wiki/concepts/cmake-build-system.md
confidence: high
updated: 2026-07-07
---

# MySQL Plugin Migration: 8.0 → 8.4

## ABI Incompatibility Is Fundamental

Plugins compiled for MySQL 8.0 **cannot** load on MySQL 8.4, and vice versa.
This is not a bug — it is the designed behaviour of `MYSQL_HANDLERTON_INTERFACE_VERSION`.

```c
// server checks on plugin load:
if ((info->interface_version >> 8) != (MYSQL_HANDLERTON_INTERFACE_VERSION >> 8))
    // reject: "API version for STORAGE ENGINE plugin is too different"
```

`MYSQL_HANDLERTON_INTERFACE_VERSION = (MYSQL_VERSION_ID << 8)` where
`MYSQL_VERSION_ID = major * 10000 + minor * 100 + patch`.

| MySQL version | MYSQL_VERSION_ID | MYSQL_HANDLERTON_INTERFACE_VERSION |
|--------------|------------------|-----------------------------------|
| 8.0.46 | 80046 | 0x13890E00 |
| 8.4.9 | 80409 | 0x13A71200 |

These values never match. Two separate plugin binaries are required: one for 8.0, one for 8.4.

## C API Changes Between 8.0 and 8.4

The `st_mysql_plugin` descriptor structure is **unchanged**.
The storage engine plugin framework is stable.

Three cost-model virtual methods are deprecated in 8.4 (warnings only, still compile):

| Deprecated | Replacement |
|-----------|-------------|
| `handler::scan_time()` | `handler::table_scan_cost()` |
| `handler::read_time()` | `handler::read_cost()` |
| `handler::key_read()` | `handler::index_scan_cost()` |

Pinba Engine does not override these methods → no action required.

## Build Requirements for Targeting 8.4

When building against MySQL 8.4 source headers:

| Requirement | 8.0 | 8.4 |
|------------|-----|-----|
| CMake (plugin only) | ≥ 3.5.1 | ≥ 3.14.6 |
| GCC | ≥ 7 | ≥ 9 |
| Clang | ≥ 5 | ≥ 11 |

Note: Boost and OpenSSL minimum version changes (for MySQL 8.4 full build) do NOT apply
when building only the plugin against downloaded headers — we don't run MySQL's cmake.

## Runtime OS Difference

| Concern | MySQL 8.0 Docker | MySQL 8.4 Docker |
|---------|-----------------|-----------------|
| Base OS | Debian bookworm | Oracle Linux 9.7 |
| Plugin dir | `/usr/lib/mysql/plugin/` | `/usr/lib64/mysql/plugin/` |
| Library dir | `/usr/lib/x86_64-linux-gnu/` | `/usr/lib64/` |
| glibc | 2.36 | 2.34 |
| libstdc++ | GLIBCXX_3.4.30 | GLIBCXX_3.4.29 |
| protobuf delivery | `libprotobuf.so.*` copied from builder | static PIC, linked into the plugin |
| Bundled server protobuf | (not an issue) | `/usr/lib64/mysql/private/libprotobuf*.so` 4.x — clashes with any dynamic protobuf in the plugin |

A plugin built on Debian bookworm loads on OL9 **only** if it uses ≤GLIBC_2.34 and
≤GLIBCXX_3.4.29 symbols — and this constraint proved too fragile to rely on. It held
when first audited, then a code change started using `std::condition_variable::wait`,
which libstdc++ 12 versions as `GLIBCXX_3.4.30`, and the published `:8.4` image
silently shipped a plugin that OL9's mysqld could not load (incident 2026-07-06;
details in [[docker-build-strategy]]).

**Rule: build the 8.4 flavor on Oracle Linux 9** (`oraclelinux:9` + `gcc-toolset-13`
for C++23; its newer libstdc++ pieces are linked statically via `libstdc++_nonshared.a`).
A symbol audit (`objdump -T ha_pinba.so | grep -oE 'GLIBC(XX)?_[0-9.]+' | sort -Vu`)
is a useful spot check but NOT sufficient — only a runtime smoke test (`INSTALL PLUGIN`
inside the actual runtime image, now part of `.github/workflows/docker.yml`) catches
this class of breakage before publishing.

## What a 8.0 → 8.4 Port Requires

1. **Download MySQL 8.4 source headers** (pinba_engine CMake handles this via `PINBA_MYSQL_SERIES=8.4`)
2. **Set `PINBA_MYSQL_SOURCE_VERSION` to the exact runtime patch** — the server
   requires an exact `MYSQL_VERSION_ID` match ("API version too different"
   otherwise). The Dockerfiles derive it from the `MYSQL_IMAGE` tag.
3. **Update runtime Dockerfile** to use the matching `mysql:8.4.x` base
4. **Fix library paths** for OL9 layout (plugin dir: `/usr/lib64/mysql/plugin/`;
   CMake installs to `/usr/local/lib64/...` on OL9, not `.../lib/...`)
5. **Link protobuf statically and skip libmysqlclient** (`PINBA_LINK_MYSQL_CLIENT=OFF`,
   `-Wl,--exclude-libs,ALL`) — mysqld 8.4 bundles its own protobuf 4.x that clashes
   with dynamic protobuf, and the image ships no libmysqlclient (details in
   [[docker-build-strategy]])
6. **Fix the `mysql_version.h` generation** (applies to both 8.0 and 8.4):
   - Source tarball ships only `mysql_version.h.in` (CMake template), not the generated `.h`
   - Without running mysql's cmake, `MYSQL_VERSION_ID` is undefined → defaults to 0 → plugin always rejected
   - CMakeLists.txt must compute `MYSQL_VERSION_ID` from `PINBA_MYSQL_SOURCE_VERSION` and write
     `builddir/include/mysql_version.h` before compilation
   - The generated file must appear first in include paths (before system MariaDB headers)

See: [[mysql-plugin-abi]], [[cmake-build-system]], [[docker-build-strategy]]
