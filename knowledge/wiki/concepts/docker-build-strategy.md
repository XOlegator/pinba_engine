---
title: "Docker Build Strategy"
type: concept
sources:
  - raw/docs/xolegator-fork-build-docker.md
  - raw/docs/mysql-plugin-api-install.md
related:
  - wiki/concepts/mysql-plugin-abi.md
  - wiki/concepts/cmake-build-system.md
  - wiki/concepts/docker-tag-strategy.md
confidence: high
updated: 2026-07-07
---

# Docker Build Strategy

## Two Dockerfiles, Not One

`Dockerfile.mysql80` and `Dockerfile.mysql84` are intentionally separate
(plus `Dockerfile.mariadb1011` / `Dockerfile.mariadb118` for the MariaDB flavor).
They are NOT parameterized into one file with `ARG MYSQL_SERIES`.

Reasons:
- Different MySQL base image pins (different SHA256 digests)
- Different `PINBA_MYSQL_SERIES` values for CMake
- Different builder OS requirements (see the GLIBCXX incident below)
- Each pipeline is independently verifiable
- Separating them prevents accidental cross-contamination

## Core Rule: Builder OS Family Must Match the Runtime Image

**The builder stage must use the same OS family (same glibc AND same system libstdc++
lineage) as the runtime base image.** Symbol-version checks of a finished binary are
not a substitute: the constraint silently breaks when new code starts using a symbol
that the newer builder toolchain versions higher (see incident below).

| Channel | Runtime base | Builder |
|---------|-------------|---------|
| MySQL 8.0 | `mysql:8.0.x-bookworm` (Debian) | `debian:bookworm` |
| MySQL 8.4 | `mysql:8.4.x` (Oracle Linux 9) | `oraclelinux:9` + `gcc-toolset-13` |
| MariaDB 10.11 / 11.8 | `mariadb:<series>` (Ubuntu) | the `mariadb:<series>` image itself |

On OL9 the system gcc is 11 (no C++23). `gcc-toolset-13` provides C++23 while linking
the newer libstdc++ pieces **statically** (`libstdc++_nonshared.a`) — the Red Hat
toolset design guarantees the result runs against the stock OL9 libstdc++. Enable it
by prepending `/opt/rh/gcc-toolset-13/root/usr/bin` to `PATH` (what `scl enable`
does); passing only `CC=/CXX=` paths breaks autoconf's secondary `$PATH` probes.
`PINBA_LINK_MYSQL_CLIENT=OFF` (CMake option) keeps `libmysqlclient` out of the
plugin's NEEDED list: the runtime image does not ship it, the server supplies those
symbols at load time, and OL9's libmysqlclient exports enough `my_*` symbols that
even `-Wl,--as-needed` would keep it. `rapidjson-devel` comes from EPEL — it is a
dependency of MySQL 8.4's **own server headers** (`sql/dd/sdi_fwd.h` includes
`rapidjson/fwd.h`), not of pinba code.

## Exact Patch Version Match Is Mandatory (MySQL flavors)

The server only loads a storage engine whose `MYSQL_VERSION_ID` equals its own
(`"API version for STORAGE ENGINE plugin is too different"` otherwise), so the
source headers must be the **same patch release** as the runtime image. Both MySQL
Dockerfiles derive `PINBA_MYSQL_SOURCE_VERSION` from the `MYSQL_IMAGE` tag in the
`cmake` step (`8.4.10` from `mysql:8.4.10`, `8.0.46` from `mysql:8.0.46-bookworm`),
so `mysql-version-monitor` bumping the tag keeps them in sync automatically.
CMakeLists drops its default source-archive MD5 (with a status message) when the
version is overridden, since that hash is recorded for the default version only.

## Incident (2026-07-06): Debian-built plugin cannot load on OL9

The published `xolegator/pinba-engine:8.4` was broken in production: `INSTALL PLUGIN`
failed with `GLIBCXX_3.4.30 not found` and the container exited during first-start init.

- Builder was `debian:bookworm` (GCC 12); runtime `mysql:8.4` is OL9 (system libstdc++
  from the GCC 11 runtime, max `GLIBCXX_3.4.29`).
- The only offending import was `std::condition_variable::wait` — libstdc++ 12 gave it
  a new symbol version `GLIBCXX_3.4.30`.
- An earlier `objdump` audit had concluded the plugin used only ≤3.4.29 symbols. That
  was true at the time, then a code change started pulling the 3.4.30 symbol and no CI
  step loaded the plugin inside the actual image, so the breakage shipped unnoticed.

Consequences baked into the pipeline:
1. Builder switched to `oraclelinux:9` + `gcc-toolset-13` (this page, rule above).
2. `.github/workflows/docker.yml` runs a **smoke test** per channel — builds the amd64
   image, starts a container, and requires `plugin_status = ACTIVE` in
   `information_schema.plugins` before anything is pushed to Docker Hub.

Fixing it surfaced three more load-time failures, each only visible at the next
stage (2026-07-07):
- **`lib64` install dir**: on OL9, CMake's GNUInstallDirs puts the plugin in
  `/usr/local/lib64/mysql/plugin/`, not `.../lib/...` — the runtime `COPY --from`
  path must match.
- **`libmysqlclient.so.21` in NEEDED**: OL9's client lib exports `my_*` symbols, so
  `--as-needed` keeps it; the runtime image doesn't ship it → `errno 11` at INSTALL
  PLUGIN. Fixed with the `PINBA_LINK_MYSQL_CLIENT=OFF` CMake option.
- **protobuf symbol clash with mysqld's bundled 4.24.4** → SIGABRT during dlopen;
  fixed by static protobuf (see "libprotobuf Delivery per Channel").
- **Patch-version mismatch** (source 8.4.9 vs runtime 8.4.10) → "API version too
  different"; fixed by deriving `PINBA_MYSQL_SOURCE_VERSION` from the image tag
  (see "Exact Patch Version Match").

## libprotobuf Delivery per Channel

- **MySQL 8.4 (OL9)**: protobuf 3.21.12 is built **from source in the builder as a
  static PIC library** (`--disable-shared --with-pic`) and linked into the plugin
  with `-Wl,--exclude-libs,ALL` (hides all archive symbols). Dynamic linking is not
  an option here: mysqld in `mysql:8.4` loads its **own bundled**
  `libprotobuf(-lite).so.24.4.0` + abseil from `/usr/lib64/mysql/private/` before the
  plugin is dlopen()ed, so any protobuf symbol the plugin imports dynamically binds
  to mysqld's copy — a version mismatch aborts the whole server inside the plugin's
  static initializers (`"This program was compiled against version 3.14.0 ... which
  is not compatible with the installed version (4.24.4)"`). Static+hidden makes the
  plugin independent of whatever protobuf Oracle bundles next. The distro
  `libprotobuf.a` is non-PIC, hence the source build.
- **MySQL 8.0 / MariaDB**: dynamic `COPY --from=builder` of `libprotobuf.so.*`
  (builder and runtime are the same OS, so the ABI matches; the Debian 8.0 mysqld
  does not leak protobuf symbols into plugins the way the OL9 8.4 one does). For
  multi-arch support the builder stages the libs into an architecture-neutral dir
  first (`cp -a /usr/lib/$(gcc -dumpmachine)/libprotobuf.so.* /staging/lib/`),
  because COPY cannot expand variables in source paths and the Debian/Ubuntu
  multiarch dir name differs between amd64 (`x86_64-linux-gnu`) and arm64
  (`aarch64-linux-gnu`). MariaDB images receive the libs in `/usr/local/lib` (in
  Ubuntu's default ld.so path; NOT in OL9's) + `ldconfig`.

## Runtime OS Differences: Debian vs Oracle Linux 9

**Critical**: `mysql:8.0.x-bookworm` is Debian bookworm. `mysql:8.4.x` is Oracle Linux 9.
The `mysql:8.0` Debian variant is published for **amd64 only** — the 8.0 channel cannot
get arm64 images; 8.4 and both MariaDB channels are amd64 + arm64.

| Concern | MySQL 8.0 (bookworm) | MySQL 8.4 (OL9) |
|---------|---------------------|-----------------|
| Plugin dir | `/usr/lib/mysql/plugin/` | `/usr/lib64/mysql/plugin/` |
| Library path | `/usr/lib/<multiarch-triplet>/` | `/usr/lib64/` (all arches) |
| glibc | 2.36 | 2.34 |
| System libstdc++ | GCC 12 / GLIBCXX_3.4.30 | GCC 11 / GLIBCXX_3.4.29 |
| `ldconfig` needed | No (DEB auto) | n/a (no libs copied; protobuf is static) |
| `/usr/local/lib` in ld path | Yes | No |

## mysql_version.h Must Be Generated by CMake

The MySQL source tarball only has `include/mysql_version.h.in` (template).
Without running cmake configure on MySQL source, `MYSQL_VERSION_ID` is undefined → `= 0` →
`MYSQL_HANDLERTON_INTERFACE_VERSION = 0`. This causes "API version too different" at load time.

CMakeLists.txt solves this by computing `MYSQL_VERSION_ID` from `PINBA_MYSQL_SOURCE_VERSION`
and writing `builddir/include/mysql_version.h` and `builddir/include/mysql/mysql_version.h`
before compilation. The `builddir/include/` is listed before the system headers in include
paths, so the generated file shadows MariaDB's compat header.

## Exposed Ports

```
3306/tcp  — MySQL
30002/udp — Pinba listener
```

Both must be exposed. The UDP port is frequently forgotten in docker-compose port mappings.

## docker-entrypoint-initdb.d Convention

MySQL Docker image runs scripts in this directory **only on first start** (when data dir is empty).
`01-init-pinba.sh` runs: INSTALL PLUGIN + CREATE DATABASE + schema import.
On subsequent starts, the plugin auto-loads from `mysql.plugin` (no init script needed).

## Plugin Filename Gotcha

| Context | Filename | Why |
|---------|----------|-----|
| CMake build output | `ha_pinba.so` | `ha_` prefix = MySQL handler convention |
| Docker runtime | `libpinba_engine.so` | Original name, used in SONAME |
| Local install | `ha_pinba.so` | Just copy with original name, use in SONAME |

When running `INSTALL PLUGIN`, the SONAME must **exactly match** the filename in plugin_dir.

See: [[mysql-plugin-abi]], [[cmake-build-system]], [[docker-tag-strategy]]
