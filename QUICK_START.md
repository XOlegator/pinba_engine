# Quick Start: Pinba Engine for MySQL 8.0

## 1. Install Dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake git pkg-config \
  libprotobuf-dev protobuf-compiler \
  libmysqlclient-dev mysql-source-8.0
```

## 2. Build

```bash
cd /path/to/pinba_engine
cmake --preset release
cmake --build --preset release
```

Expected result: `build/ha_pinba.so`.

## 3. Install Plugin

```bash
MYSQL_PLUGIN_DIR=$(mysql -u root -p -e "SHOW VARIABLES LIKE 'plugin_dir';" 2>/dev/null | grep plugin_dir | awk '{print $2}')
[ -z "$MYSQL_PLUGIN_DIR" ] && MYSQL_PLUGIN_DIR="/usr/lib/mysql/plugin"

sudo cp build/ha_pinba.so "$MYSQL_PLUGIN_DIR/"
sudo chmod 644 "$MYSQL_PLUGIN_DIR/ha_pinba.so"
sudo chown mysql:mysql "$MYSQL_PLUGIN_DIR/ha_pinba.so"
```

## 4. Enable Plugin

```sql
INSTALL PLUGIN pinba SONAME 'ha_pinba.so';
SHOW PLUGINS;
```

## 5. Create Schema

```bash
mysql -u root -p -e "CREATE DATABASE IF NOT EXISTS pinba;"
mysql -u root -p pinba < default_tables.sql
```

## Troubleshooting

If CMake cannot find MySQL server headers:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPINBA_MYSQL_SOURCE_DIR=/path/to/mysql-8.0
```

Or opt in to downloading the pinned MySQL source archive:

```bash
cmake --preset release-download-mysql-source
```
