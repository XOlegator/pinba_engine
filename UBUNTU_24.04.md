# Ubuntu 24.04 Build Guide

This guide targets Ubuntu/Kubuntu 24.04 with MySQL 8.0 and CMake.

## 1. Install Dependencies

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake git pkg-config \
  libprotobuf-dev protobuf-compiler \
  libmysqlclient-dev mysql-source-8.0 \
  mysql-server mysql-client-8.0
```

## 2. Build

```bash
cd /path/to/pinba_engine
cmake --preset release
cmake --build --preset release
```

## 3. Install Plugin

```bash
sudo cp build/ha_pinba.so /usr/lib/mysql/plugin/
sudo chmod 644 /usr/lib/mysql/plugin/ha_pinba.so
sudo chown mysql:mysql /usr/lib/mysql/plugin/ha_pinba.so
```

## 4. Enable Plugin

```bash
mysql -u root -p -e "INSTALL PLUGIN pinba SONAME 'ha_pinba.so';"
```

## 5. Initialize Schema

```bash
mysql -u root -p -e "CREATE DATABASE IF NOT EXISTS pinba;"
mysql -u root -p pinba < default_tables.sql
```

## Validation

```sql
SHOW PLUGINS;
SELECT PLUGIN_NAME, PLUGIN_STATUS
FROM INFORMATION_SCHEMA.PLUGINS
WHERE PLUGIN_NAME = 'PINBA';
```
