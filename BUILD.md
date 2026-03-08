# Build Guide: Pinba Engine for MySQL 8.0

## Quick Start

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake git \
  libprotobuf-dev protobuf-compiler \
  libmysqlclient-dev mysql-source-8.0 pkg-config

mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
```

The resulting plugin should be `ha_pinba.so` in the build directory.

## Requirements

- Linux (Ubuntu 20.04+, Debian 11+, CentOS/RHEL 8+)
- MySQL 8.0+
- GCC 9+ or Clang 10+
- CMake 3.16+
- Protocol Buffers 3.0+

## Installing the plugin

```bash
sudo cp ha_pinba.so /usr/lib/mysql/plugin/
sudo chmod 644 /usr/lib/mysql/plugin/ha_pinba.so
sudo chown mysql:mysql /usr/lib/mysql/plugin/ha_pinba.so

mysql -u root -p -e "INSTALL PLUGIN pinba SONAME 'ha_pinba.so';"
```

## Notes about MySQL headers

A production-ready build requires MySQL **server** headers (`sql/handler.h` and related files).

Recommended source:
- package `mysql-source-8.0`
- or a manually extracted MySQL 8.0 source tree

You can pass the path explicitly:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DPINBA_MYSQL_SOURCE_DIR=/path/to/mysql-8.0
```

## Clean rebuild

```bash
rm -rf build cmake-build-*
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
```
