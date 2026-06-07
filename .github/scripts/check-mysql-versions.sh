#!/usr/bin/env bash
set -euo pipefail

suite="${1:?usage: check-mysql-versions.sh <ubuntu-suite>}"

extract_latest_series_version() {
  local series="$1"
  local package="$2"

  apt-cache madison "$package" 2>/dev/null \
    | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' \
    | grep "^${series}\\." \
    | sort -V \
    | tail -n 1 \
    || true
}

apt-get update -q >/dev/null

mysql80_version="$(extract_latest_series_version "8.0" libmysqlclient-dev)"
mysql84_version="$(extract_latest_series_version "8.4" libmysqlclient-dev)"

if [[ -z "$mysql80_version" ]]; then
  mysql80_version="null"
else
  mysql80_version="\"$mysql80_version\""
fi

if [[ -z "$mysql84_version" ]]; then
  mysql84_version="null"
else
  mysql84_version="\"$mysql84_version\""
fi

cat <<EOF
{
  "suite": "$suite",
  "mysql80": $mysql80_version,
  "mysql84": $mysql84_version
}
EOF
