# Build Guide: Pinba Engine for MySQL 8.0

## Requirements

- Linux with MySQL 8.0 client development files and server source headers.
- CMake 3.24+.
- GCC 13+ or a Clang version with practical C++23 support.
- Protocol Buffers 3.x.

On Ubuntu 24.04:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake git pkg-config \
  libprotobuf-dev protobuf-compiler \
  libmysqlclient-dev mysql-source-8.0
```

## Build

Prefer CMake presets:

```bash
cmake --preset release
cmake --build --preset release
```

Or use explicit commands:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The resulting plugin is `build/ha_pinba.so`.

## MySQL Server Headers

A plugin build requires MySQL server headers such as `sql/handler.h`, not only client headers.

Use one of these sources:

- distro package such as `mysql-source-8.0`;
- an extracted MySQL 8.0 source archive;
- the pinned CMake download path via `-DPINBA_DOWNLOAD_MYSQL_SOURCE=ON`.

Explicit source tree:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPINBA_MYSQL_SOURCE_DIR=/path/to/mysql-8.0
```

Opt-in download:

```bash
cmake --preset release-download-mysql-source
```

## Checks

```bash
cmake --build build -j"$(nproc)"
./build/pinba_test
ctest --test-dir build --output-on-failure
```

## Installing the Plugin

```bash
sudo cp build/ha_pinba.so /usr/lib/mysql/plugin/
sudo chmod 644 /usr/lib/mysql/plugin/ha_pinba.so
sudo chown mysql:mysql /usr/lib/mysql/plugin/ha_pinba.so
mysql -u root -p -e "INSTALL PLUGIN pinba SONAME 'ha_pinba.so';"
```

## Clean Rebuild

```bash
rm -rf build build-* cmake-build-*
cmake --preset release
cmake --build --preset release
```
