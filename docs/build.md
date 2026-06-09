# Build and Development Guide

This project builds a MySQL 8.0+ storage engine plugin with CMake and C++23.
Supported target series:

- MySQL 8.0 (current stable compatibility line)
- MySQL 8.4 LTS

## Requirements

Required tools and libraries:

- Linux development environment.
- CMake 3.24 or newer.
- GCC 13+ or a Clang version with practical C++23 support.
- Protocol Buffers 3.x (`protoc`, headers, and library).
- MySQL 8.0 or 8.4 client development files (`mysql.h`, `libmysqlclient`).
- MySQL 8.0 or 8.4 server source headers containing `sql/handler.h`.

Ubuntu 24.04 example:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake git pkg-config \
  libprotobuf-dev protobuf-compiler \
  libmysqlclient-dev mysql-source-8.0 \
  mysql-server mysql-client-8.0
```

## Protocol Buffers

The engine decodes the Pinba UDP wire format with the **C++ Protocol Buffers**
runtime (`libprotobuf`, 3.x). CMake locates it via `find_package(Protobuf)` and
generates `pinba.pb.{h,cc}` from `pinba.proto` with `protobuf_generate_cpp`.

C++ protobuf is used on purpose: the engine is a MySQL 8 plugin built with
CMake/C++23, and MySQL itself ships and links the C++ protobuf runtime, so
reusing it keeps the toolchain and ABI aligned with the host server.

The legacy data path (`data.cc`, `ha_pinba.cc`, `pool.cc`, ...) works on plain
C-style structs (`Pinba__Request` and friends) declared in the hand-written
`src/pinba.pb-c.h`. `src/pinba.pb-c.cc` is a thin shim that converts a decoded
C++ `Pinba::Request` into that flat C struct, so the data path did not have to
be rewritten during the MySQL 8 migration. This is **not** the protobuf-c (C)
runtime — only the struct shape resembles it.

Relationship with the PHP extension: the
[pinba_extension](https://github.com/XOlegator/pinba_extension) client
genuinely uses the protobuf-c (C) runtime, which is the right fit for a lean
PHP C extension. The two projects intentionally use different protobuf runtimes;
the only shared, stable contract is `pinba.proto`. Keep both copies of
`pinba.proto` field-compatible — only append fields, never renumber or retype.

## MySQL Server Headers

A plugin build requires MySQL server headers, not only the client development package.
Use one of these sources:

- distro package such as `mysql-source-8.0` or `mysql-source-8.4`;
- an extracted MySQL source archive;
- the pinned CMake download path via `-DPINBA_DOWNLOAD_MYSQL_SOURCE=ON`.

Explicit source tree:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPINBA_MYSQL_SOURCE_DIR=/path/to/mysql-8.4.9
```

Opt-in source download:

```bash
cmake --preset release-download-mysql-source
# MySQL 8.4 LTS headers
cmake --preset release-download-mysql84-source
```

The MySQL source archive is large, so downloading it is intentionally opt-in. Pinned version, URL, and hash are defined in `CMakeLists.txt` per target series.

## Select target MySQL series

Default target series is `8.0`.
Set `PINBA_MYSQL_SERIES` for LTS 8.4 builds:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPINBA_MYSQL_SERIES=8.4
cmake --build build -j"$(nproc)"
```

## Build

Preferred command:

```bash
cmake --preset release
cmake --build --preset release
```

Equivalent explicit command:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Expected output: `build/ha_pinba.so`.

## Useful Build Options

```bash
# Debug build with Pinba debug logging
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DPINBA_DEBUG=ON

# Debug build with sanitizers
cmake --preset debug

# Unit tests, enabled by default
cmake -S . -B build -DPINBA_WITH_TESTS=ON

# Benchmarks
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DPINBA_WITH_BENCHMARKS=ON
```

## Checks

Minimum check after code changes:

```bash
cmake --build build -j"$(nproc)"
./build/pinba_test
ctest --test-dir build --output-on-failure
```

Static analysis and repository hooks:

```bash
./scripts/run_clang_tidy.sh
pre-commit run --all-files
```

CMake generates `build/compile_commands.json` by default. The pre-push hooks also use `cppcheck` and `clang-tidy`; install the tools locally when working on C/C++ changes.

## Install Plugin Locally

Use the MySQL-reported plugin directory instead of hard-coding a distro path:

```bash
MYSQL_PLUGIN_DIR=$(mysql --user=root --password --batch --skip-column-names --raw --execute="SELECT @@global.plugin_dir;")
printf 'MySQL plugin dir: %s\n' "$MYSQL_PLUGIN_DIR"
test -n "$MYSQL_PLUGIN_DIR"
sudo cp build/ha_pinba.so "$MYSQL_PLUGIN_DIR/"
sudo chmod 644 "$MYSQL_PLUGIN_DIR/ha_pinba.so"
sudo chown mysql:mysql "$MYSQL_PLUGIN_DIR/ha_pinba.so"
mysql -u root -p -e "INSTALL PLUGIN pinba SONAME 'ha_pinba.so';"
```

If `INSTALL PLUGIN` returns `ERROR 1125 (HY000): Function 'pinba' already exists`, the plugin is already registered in MySQL. For a rebuild/reinstall, unload it first, replace the shared object, and install it again:

```bash
mysql -u root -p -e "UNINSTALL PLUGIN pinba;"
MYSQL_PLUGIN_DIR=$(mysql --user=root --password --batch --skip-column-names --raw --execute="SELECT @@global.plugin_dir;")
printf 'MySQL plugin dir: %s\n' "$MYSQL_PLUGIN_DIR"
test -n "$MYSQL_PLUGIN_DIR"
sudo cp build/ha_pinba.so "$MYSQL_PLUGIN_DIR/"
sudo chmod 644 "$MYSQL_PLUGIN_DIR/ha_pinba.so"
sudo chown mysql:mysql "$MYSQL_PLUGIN_DIR/ha_pinba.so"
mysql -u root -p -e "INSTALL PLUGIN pinba SONAME 'ha_pinba.so';"
```

If MySQL refuses to unload the plugin because it is in use, stop active sessions using Pinba tables and retry. If needed during local development, restart MySQL after `UNINSTALL PLUGIN` and before replacing `ha_pinba.so`.

To check whether Pinba is already installed:

```bash
mysql --user=root --password --table --execute="SELECT PLUGIN_NAME, PLUGIN_STATUS FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME = 'PINBA';"
```

## Initialize Schema

```bash
mysql -u root -p -e "CREATE DATABASE IF NOT EXISTS pinba;"
mysql -u root -p pinba < default_tables.sql
```

## Validate Plugin

```sql
SHOW PLUGINS;
SELECT PLUGIN_NAME, PLUGIN_STATUS
FROM INFORMATION_SCHEMA.PLUGINS
WHERE PLUGIN_NAME = 'PINBA';
```

## Clean Rebuild

```bash
rm -rf build build-* cmake-build-*
cmake --preset release
cmake --build --preset release
```
