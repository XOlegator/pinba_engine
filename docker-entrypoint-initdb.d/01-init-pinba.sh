#!/bin/bash
# Initialize Pinba Engine in MySQL container

set -e

echo "Initializing Pinba Engine..."

# Wait until MySQL is ready
until mysqladmin ping -h localhost --silent; do
    echo "Waiting for MySQL to be ready..."
    sleep 1
done

# Install plugin
mysql -u root -p"$MYSQL_ROOT_PASSWORD" <<EOF_SQL
INSTALL PLUGIN pinba SONAME 'libpinba_engine.so';
EOF_SQL

# Create schema if it does not exist
mysql -u root -p"$MYSQL_ROOT_PASSWORD" <<EOF_SQL
CREATE DATABASE IF NOT EXISTS pinba;
USE pinba;
SOURCE /usr/share/pinba_engine/default_tables.sql;
EOF_SQL

echo "Pinba Engine initialized successfully!"
