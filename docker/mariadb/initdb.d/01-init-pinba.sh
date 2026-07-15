#!/bin/bash
# Initialize Pinba Engine in a MariaDB container.

set -e

echo "Initializing Pinba Engine..."
# The MariaDB image accepts both MARIADB_* and MYSQL_* env vars.
export MYSQL_PWD="${MARIADB_ROOT_PASSWORD:-${MYSQL_ROOT_PASSWORD}}"

# MariaDB 11.x ships the `mariadb`/`mariadb-admin` clients (the `mysql`-named
# tools are being phased out), so use those.
until mariadb-admin ping -h localhost --silent; do
    echo "Waiting for MariaDB to be ready..."
    sleep 1
done

# Install the plugin (persisted in mysql.plugin, so it survives restarts).
mariadb -u root <<'EOF_SQL'
INSTALL PLUGIN pinba SONAME 'libpinba_engine.so';
EOF_SQL

# Create the schema if it does not exist yet.
mariadb -u root <<'EOF_SQL'
CREATE DATABASE IF NOT EXISTS pinba;
USE pinba;
SOURCE /usr/share/pinba_engine/default_tables.sql;
EOF_SQL

echo "Pinba Engine initialized successfully!"
