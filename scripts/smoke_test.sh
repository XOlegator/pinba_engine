#!/usr/bin/env bash
# SQL-level smoke test for Pinba Engine.
# Requires a running MySQL instance with the PINBA plugin loaded.
# Usage:
#   ./scripts/smoke_test.sh [host] [port] [user] [password]
# Defaults match the Docker development setup (root, no password, 3306).

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-3306}"
USER="${3:-root}"
PASS="${4:-}"
DB="${5:-pinba}"

# Client binary is overridable (MariaDB hosts may ship only `mariadb`, not the
# `mysql` compat symlink — especially under sudo's restricted PATH).
MYSQL="${MYSQL_CLIENT:-mysql} --host=${HOST} --port=${PORT} --user=${USER} ${PASS:+--password=${PASS}} --batch --skip-column-names"

pass() { printf '\033[32mPASS\033[0m %s\n' "$1"; }
fail() { printf '\033[31mFAIL\033[0m %s\n' "$1"; exit 1; }

echo "=== Pinba Engine SQL smoke test ==="
echo "Target: ${USER}@${HOST}:${PORT}/${DB}"
echo

# ── 0. Preflight: client binary + connectivity ──────────────────────────────
# Fail loudly here instead of dying silently later: the per-query pipelines below
# route client stderr to /dev/null, so a missing client binary or a connection
# error would otherwise abort the script under `set -e` with no diagnostic.
CLIENT_BIN="${MYSQL_CLIENT:-mysql}"
command -v "${CLIENT_BIN}" >/dev/null 2>&1 \
    || fail "client binary '${CLIENT_BIN}' not found in PATH (set MYSQL_CLIENT to the right client, e.g. mariadb)"
if ! ${MYSQL} -e "SELECT 1;" >/dev/null; then
    fail "cannot connect to ${USER}@${HOST}:${PORT} using '${CLIENT_BIN}' (see client error above)"
fi
pass "connected via '${CLIENT_BIN}'"

# ── 1. Plugin active ────────────────────────────────────────────────────────
status=$(${MYSQL} -e "SELECT PLUGIN_STATUS FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME='PINBA';" 2>/dev/null | head -1)
[[ "${status}" == "ACTIVE" ]] || fail "PINBA plugin not ACTIVE (got: '${status}')"
pass "plugin ACTIVE"

# ── 2. Core virtual tables readable ─────────────────────────────────────────
for tbl in info status request tag timer timertag active_reports; do
    row=$(${MYSQL} "${DB}" -e "SELECT COUNT(*) FROM ${tbl};" 2>/dev/null | head -1)
    [[ "${row}" =~ ^[0-9]+$ ]] || fail "${tbl}: SELECT COUNT(*) returned '${row}'"
    pass "${tbl} readable"
done

# ── 3. status.build_string is set ───────────────────────────────────────────
build=$(${MYSQL} "${DB}" -e "SELECT build_string FROM status;" 2>/dev/null | head -1)
[[ -n "${build}" && "${build}" != "NULL" ]] || fail "status.build_string is NULL — plugin version not compiled in"
pass "status.build_string = ${build}"

# ── 4. Standard report tables readable ──────────────────────────────────────
for tbl in \
    report_by_script_name report_by_server_name report_by_hostname \
    report_by_server_and_script report_by_hostname_and_script \
    report_by_hostname_and_server report_by_hostname_server_and_script \
    report_by_status report_by_script_and_status report_by_server_and_status \
    report_by_hostname_and_status report_by_hostname_script_and_status \
    report_by_schema report_by_script_and_schema report_by_server_and_schema \
    report_by_hostname_and_schema report_by_hostname_script_and_schema \
    report_by_hostname_status_and_schema; do
    row=$(${MYSQL} "${DB}" -e "SELECT COUNT(*) FROM ${tbl};" 2>/dev/null | head -1)
    [[ "${row}" =~ ^[0-9]+$ ]] || fail "${tbl}: SELECT COUNT(*) returned '${row}'"
done
pass "all 18 report tables readable"

# ── 5. Dynamic tag_report tables (create / select / drop) ───────────────────
SMOKE_TAG="__smoke_test_tag"

${MYSQL} "${DB}" -e "
DROP TABLE IF EXISTS \`_smoke_tag_report\`;
CREATE TABLE \`_smoke_tag_report\` (
  \`script_name\`  varchar(128) DEFAULT NULL,
  \`tag_value\`    varchar(64)  DEFAULT NULL,
  \`req_count\`    int          DEFAULT NULL,
  \`req_per_sec\`  float        DEFAULT NULL,
  \`hit_count\`    int          DEFAULT NULL,
  \`hit_per_sec\`  float        DEFAULT NULL,
  \`timer_value\`  float        DEFAULT NULL,
  \`timer_median\` float        DEFAULT NULL,
  \`ru_utime_value\` float      DEFAULT NULL,
  \`ru_stime_value\` float      DEFAULT NULL,
  \`index_value\`  varchar(256) DEFAULT NULL
) ENGINE=PINBA DEFAULT CHARSET=latin1 COMMENT='tag_report:${SMOKE_TAG}';
"
row=$(${MYSQL} "${DB}" -e "SELECT COUNT(*) FROM \`_smoke_tag_report\`;" 2>/dev/null | head -1)
[[ "${row}" =~ ^[0-9]+$ ]] || fail "tag_report: SELECT COUNT(*) returned '${row}'"
${MYSQL} "${DB}" -e "DROP TABLE IF EXISTS \`_smoke_tag_report\`;" 2>/dev/null
pass "tag_report (1 tag) create/select/drop"

${MYSQL} "${DB}" -e "
DROP TABLE IF EXISTS \`_smoke_tagN_info\`;
CREATE TABLE \`_smoke_tagN_info\` (
  \`tag1_value\`    varchar(64)  DEFAULT NULL,
  \`tag2_value\`    varchar(64)  DEFAULT NULL,
  \`req_count\`     int          DEFAULT NULL,
  \`req_per_sec\`   float        DEFAULT NULL,
  \`hit_count\`     int          DEFAULT NULL,
  \`hit_per_sec\`   float        DEFAULT NULL,
  \`timer_value\`   float        DEFAULT NULL,
  \`timer_median\`  float        DEFAULT NULL,
  \`ru_utime_value\` float       DEFAULT NULL,
  \`ru_stime_value\` float       DEFAULT NULL,
  \`index_value\`   varchar(256) DEFAULT NULL
) ENGINE=PINBA DEFAULT CHARSET=latin1 COMMENT='tagN_info:${SMOKE_TAG},__server_name::';
"
row=$(${MYSQL} "${DB}" -e "SELECT COUNT(*) FROM \`_smoke_tagN_info\`;" 2>/dev/null | head -1)
[[ "${row}" =~ ^[0-9]+$ ]] || fail "tagN_info: SELECT COUNT(*) returned '${row}'"
${MYSQL} "${DB}" -e "DROP TABLE IF EXISTS \`_smoke_tagN_info\`;" 2>/dev/null
pass "tagN_info (N tags) create/select/drop"

# ── 6. Percentile report variant ─────────────────────────────────────────────
${MYSQL} "${DB}" -e "
DROP TABLE IF EXISTS \`_smoke_report_pct\`;
CREATE TABLE \`_smoke_report_pct\` (
  \`req_count\`              int   DEFAULT NULL,
  \`req_per_sec\`            float DEFAULT NULL,
  \`req_time_total\`         float DEFAULT NULL,
  \`req_time_percent\`       float DEFAULT NULL,
  \`req_time_per_sec\`       float DEFAULT NULL,
  \`ru_utime_total\`         float DEFAULT NULL,
  \`ru_utime_percent\`       float DEFAULT NULL,
  \`ru_utime_per_sec\`       float DEFAULT NULL,
  \`ru_stime_total\`         float DEFAULT NULL,
  \`ru_stime_percent\`       float DEFAULT NULL,
  \`ru_stime_per_sec\`       float DEFAULT NULL,
  \`traffic_total\`          float DEFAULT NULL,
  \`traffic_percent\`        float DEFAULT NULL,
  \`traffic_per_sec\`        float DEFAULT NULL,
  \`script_name\`            varchar(128) DEFAULT NULL,
  \`memory_footprint_total\` float DEFAULT NULL,
  \`memory_footprint_percent\` float DEFAULT NULL,
  \`req_time_median\`        float DEFAULT NULL,
  \`index_value\`            varchar(256) DEFAULT NULL,
  \`p90\`                    float DEFAULT NULL,
  \`p95\`                    float DEFAULT NULL,
  \`p99\`                    float DEFAULT NULL
) ENGINE=PINBA DEFAULT CHARSET=latin1 COMMENT='report1:::90,95,99';
"
row=$(${MYSQL} "${DB}" -e "SELECT COUNT(*) FROM \`_smoke_report_pct\`;" 2>/dev/null | head -1)
[[ "${row}" =~ ^[0-9]+$ ]] || fail "report1 with percentiles: SELECT COUNT(*) returned '${row}'"
${MYSQL} "${DB}" -e "DROP TABLE IF EXISTS \`_smoke_report_pct\`;" 2>/dev/null
pass "report with percentiles (:::90,95,99) create/select/drop"

echo
echo "=== All smoke tests passed ==="
