#!/usr/bin/env bash
# Data-driven percentile field-mapping test for Pinba Engine.
#
# Regression test for the report3 off-by-one: report3_fetch_row used to map
# index_value to field 19 and percentiles from field 20, while the report3
# schema (identical to report2) has index_value at field 18. As a result the
# p90 column received the hostname string (stored into FLOAT as garbage),
# p95 held the true p90, p99 held the true p95 and index_value was NULL.
#
# The test creates percentile variants of report2 and report3, drives them
# with real UDP traffic via scripts/send_pinba_packet.py, and asserts that in
# every row index_value mirrors the key column and p90/p95/p99 are sane,
# positive and monotonic.
#
# Requires a running MySQL instance with the PINBA plugin loaded and a
# reachable collector UDP port.
# Usage:
#   ./scripts/percentile_mapping_test.sh [host] [port] [user] [password] [db]
# Env:
#   MYSQL_CLIENT  client binary (default: mysql; MariaDB hosts may need mariadb)
#   UDP_HOST      collector host for the packet sender (default: same as host)
#   UDP_PORT      collector UDP port (default: 30002)

set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-3306}"
USER="${3:-root}"
PASS="${4:-}"
DB="${5:-pinba}"
UDP_HOST="${UDP_HOST:-${HOST}}"
UDP_PORT="${UDP_PORT:-30002}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

MYSQL="${MYSQL_CLIENT:-mysql} --host=${HOST} --port=${PORT} --user=${USER} ${PASS:+--password=${PASS}} --batch --skip-column-names"

pass() { printf '\033[32mPASS\033[0m %s\n' "$1"; }
fail() { printf '\033[31mFAIL\033[0m %s\n' "$1"; exit 1; }

cleanup() {
    ${MYSQL} "${DB}" -e "DROP TABLE IF EXISTS \`_pctmap_report2\`; DROP TABLE IF EXISTS \`_pctmap_report3\`;" 2>/dev/null || true
}
trap cleanup EXIT

echo "=== Pinba Engine percentile field-mapping test ==="
echo "Target: ${USER}@${HOST}:${PORT}/${DB}, collector ${UDP_HOST}:${UDP_PORT}/udp"
echo

# ── 0. Preflight ────────────────────────────────────────────────────────────
CLIENT_BIN="${MYSQL_CLIENT:-mysql}"
command -v "${CLIENT_BIN}" >/dev/null 2>&1 \
    || fail "client binary '${CLIENT_BIN}' not found in PATH (set MYSQL_CLIENT)"
${MYSQL} -e "SELECT 1;" >/dev/null || fail "cannot connect to ${USER}@${HOST}:${PORT}"
command -v python3 >/dev/null 2>&1 || fail "python3 not found in PATH (needed for the packet sender)"
pass "preflight"

# ── 1. Create percentile report variants (report2 + report3) ────────────────
# Reports are lazy: they only aggregate packets received after the table is
# created and first selected from, so create + activate before sending.
# shellcheck disable=SC2016  # backticks are literal SQL quoting, not expansions
common_cols='
  `req_count`                int   DEFAULT NULL,
  `req_per_sec`              float DEFAULT NULL,
  `req_time_total`           float DEFAULT NULL,
  `req_time_percent`         float DEFAULT NULL,
  `req_time_per_sec`         float DEFAULT NULL,
  `ru_utime_total`           float DEFAULT NULL,
  `ru_utime_percent`         float DEFAULT NULL,
  `ru_utime_per_sec`         float DEFAULT NULL,
  `ru_stime_total`           float DEFAULT NULL,
  `ru_stime_percent`         float DEFAULT NULL,
  `ru_stime_per_sec`         float DEFAULT NULL,
  `traffic_total`            float DEFAULT NULL,
  `traffic_percent`          float DEFAULT NULL,
  `traffic_per_sec`          float DEFAULT NULL,'
# shellcheck disable=SC2016  # same: literal SQL backticks
tail_cols='
  `memory_footprint_total`   float DEFAULT NULL,
  `memory_footprint_percent` float DEFAULT NULL,
  `req_time_median`          float DEFAULT NULL,
  `index_value`              varchar(256) DEFAULT NULL,
  `p90`                      float DEFAULT NULL,
  `p95`                      float DEFAULT NULL,
  `p99`                      float DEFAULT NULL'

${MYSQL} "${DB}" -e "
DROP TABLE IF EXISTS \`_pctmap_report2\`;
CREATE TABLE \`_pctmap_report2\` (${common_cols}
  \`server_name\` varchar(64) DEFAULT NULL,${tail_cols}
) ENGINE=PINBA DEFAULT CHARSET=latin1 COMMENT='report2:::90,95,99';
DROP TABLE IF EXISTS \`_pctmap_report3\`;
CREATE TABLE \`_pctmap_report3\` (${common_cols}
  \`hostname\` varchar(64) DEFAULT NULL,${tail_cols}
) ENGINE=PINBA DEFAULT CHARSET=latin1 COMMENT='report3:::90,95,99';
SELECT COUNT(*) FROM \`_pctmap_report2\`;
SELECT COUNT(*) FROM \`_pctmap_report3\`;
" >/dev/null
pass "report2/report3 percentile tables created and activated"

# ── 2. Drive them with real UDP traffic ─────────────────────────────────────
python3 "${SCRIPT_DIR}/send_pinba_packet.py" --host "${UDP_HOST}" --port "${UDP_PORT}" --count 300 >/dev/null
pass "300 packets sent to ${UDP_HOST}:${UDP_PORT}"

rows=0
for _ in $(seq 1 30); do
    rows=$(${MYSQL} "${DB}" -e "SELECT COUNT(*) FROM \`_pctmap_report3\`;" 2>/dev/null | head -1)
    [[ "${rows:-0}" -gt 0 ]] && break
    sleep 1
done
[[ "${rows:-0}" -gt 0 ]] || fail "report3 percentile table did not aggregate any data"
pass "report3 percentile table populated (${rows} rows)"

# ── 3. Field-mapping assertions ─────────────────────────────────────────────
# In every row: index_value mirrors the key column (it is NULL when the fetch
# numbering is shifted), and p90/p95/p99 are positive, monotonic floats (the
# shifted numbering stores the key string into p90, which becomes 0/garbage).
check_table() {
    local table="$1" key_col="$2"
    local total bad
    total=$(${MYSQL} "${DB}" -e "SELECT COUNT(*) FROM \`${table}\`;" | head -1)
    [[ "${total:-0}" -gt 0 ]] || fail "${table}: no rows to verify"
    bad=$(${MYSQL} "${DB}" -e "
        SELECT COUNT(*) FROM \`${table}\`
        WHERE NOT (index_value <=> ${key_col})
           OR p90 IS NULL OR p95 IS NULL OR p99 IS NULL
           OR p90 <= 0 OR p90 > p95 OR p95 > p99;" | head -1)
    [[ "${bad}" == "0" ]] || {
        ${MYSQL} "${DB}" -e "SELECT ${key_col}, index_value, req_time_median, p90, p95, p99 FROM \`${table}\`;" || true
        fail "${table}: ${bad}/${total} rows have broken index_value/percentile mapping"
    }
    pass "${table}: ${total} rows, index_value == ${key_col}, 0 < p90 <= p95 <= p99"
}

check_table "_pctmap_report2" "server_name"
check_table "_pctmap_report3" "hostname"

echo
echo "=== Percentile field-mapping test passed ==="
