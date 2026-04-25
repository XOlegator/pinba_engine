# Build and Development Guide

This project builds a MySQL 8.0+ storage engine plugin with CMake and C++23.

## Requirements

Required tools and libraries:

- Linux development environment.
- CMake 3.24 or newer.
- GCC 13+ or a Clang version with practical C++23 support.
- Protocol Buffers 3.x (`protoc`, headers, and library).
- MySQL 8.0 client development files (`mysql.h`, `libmysqlclient`).
- MySQL 8.0 server source headers containing `sql/handler.h`.

Ubuntu 24.04 example:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake git pkg-config \
  libprotobuf-dev protobuf-compiler \
  libmysqlclient-dev mysql-source-8.0 \
  mysql-server mysql-client-8.0
```

## MySQL Server Headers

A plugin build requires MySQL server headers, not only the client development package.
Use one of these sources:

- distro package such as `mysql-source-8.0`;
- an extracted MySQL 8.0 source archive;
- the pinned CMake download path via `-DPINBA_DOWNLOAD_MYSQL_SOURCE=ON`.

Explicit source tree:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPINBA_MYSQL_SOURCE_DIR=/path/to/mysql-8.0
```

Opt-in source download:

```bash
cmake --preset release-download-mysql-source
```

The MySQL source archive is large, so downloading it is intentionally opt-in. The pinned version, URL, and hash are defined in `CMakeLists.txt`.

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

```bash
sudo cp build/ha_pinba.so /usr/lib/mysql/plugin/
sudo chmod 644 /usr/lib/mysql/plugin/ha_pinba.so
sudo chown mysql:mysql /usr/lib/mysql/plugin/ha_pinba.so
mysql -u root -p -e "INSTALL PLUGIN pinba SONAME 'ha_pinba.so';"
```

If the plugin directory differs:

```bash
MYSQL_PLUGIN_DIR=$(mysql -u root -p -e "SHOW VARIABLES LIKE 'plugin_dir';" 2>/dev/null | grep plugin_dir | awk '{print $2}')
sudo cp build/ha_pinba.so "$MYSQL_PLUGIN_DIR/"
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
