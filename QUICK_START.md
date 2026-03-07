# Quick Start: Pinba Engine for MySQL 8.0

## 1. Install dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake \
  libprotobuf-dev protobuf-compiler \
  libmysqlclient-dev mysql-source-8.0 pkg-config
```

## 2. Build

```bash
cd /path/to/pinba_engine
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
```

Expected result: `ha_pinba.so` is created in `build/`.

## 3. Install plugin

```bash
MYSQL_PLUGIN_DIR=$(mysql -u root -p -e "SHOW VARIABLES LIKE 'plugin_dir';" 2>/dev/null | grep plugin_dir | awk '{print $2}')
[ -z "$MYSQL_PLUGIN_DIR" ] && MYSQL_PLUGIN_DIR="/usr/lib/mysql/plugin"

sudo cp ha_pinba.so "$MYSQL_PLUGIN_DIR/"
sudo chmod 644 "$MYSQL_PLUGIN_DIR/ha_pinba.so"
sudo chown mysql:mysql "$MYSQL_PLUGIN_DIR/ha_pinba.so"
```

## 4. Enable plugin in MySQL

```sql
INSTALL PLUGIN pinba SONAME 'ha_pinba.so';
SHOW PLUGINS;
```

## 5. Create schema

```bash
mysql -u root -p -e "CREATE DATABASE IF NOT EXISTS pinba;"
mysql -u root -p pinba < ../default_tables.sql
```

## Troubleshooting

If CMake cannot find MySQL server headers:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DPINBA_MYSQL_SOURCE_DIR=/path/to/mysql-8.0
```
