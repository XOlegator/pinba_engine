#!/usr/bin/env python3
"""Compare filesort row references with an unsorted snapshot of live Pinba rows.

Requires default_tables.sql and a reachable collector. For example:
  python3 scripts/row_position_test.py --mysql-option=--socket=/tmp/mysql.sock \
      --udp-port=31002

Use a disposable server with 100-slot request/timer pools, stats_history=10 and
--warmup=80 --warmup-delay=11 to exercise nonzero pool offsets and wraparound.
MYSQL_PWD supplies the password.
"""

import argparse
from decimal import Decimal
from pathlib import Path
import socket
import subprocess
import sys
import time
import uuid

from send_pinba_packet import (
    F_HOSTNAME, F_REQUEST_TIME, F_SCRIPT_NAME, F_SERVER_NAME,
    _float, _string, build_request,
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mysql-client", default="mysql")
    parser.add_argument("--mysql-option", action="append", default=[])
    parser.add_argument("--database", default="pinba")
    parser.add_argument("--udp-host", default="127.0.0.1")
    parser.add_argument("--udp-port", type=int, default=30002)
    parser.add_argument("--warmup", type=int, default=0)
    parser.add_argument("--warmup-delay", type=float, default=0)
    args = parser.parse_args()
    client = [args.mysql_client, *args.mysql_option, "--batch", "--skip-column-names", args.database]

    def query(sql, row_references=False):
        # MariaDB and MySQL 8.0 can switch between addon-field and rowid filesort.
        # MySQL 8.4 removed this setting; there the default path is still tested.
        prefix = "SET SESSION max_length_for_sort_data=0; " if row_references else ""
        result = subprocess.run(client, input=prefix + sql, text=True, capture_output=True, timeout=10)
        if result.returncode and row_references and "Unknown system variable" in result.stderr:
            result = subprocess.run(client, input=sql, text=True, capture_output=True, timeout=10)
        if result.returncode:
            raise RuntimeError(result.stderr.strip())
        return [line.split("\t") for line in result.stdout.splitlines()]

    marker = "rowpos_" + uuid.uuid4().hex
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        for i in range(args.warmup):
            sock.sendto(build_request(i), (args.udp_host, args.udp_port))
            time.sleep(0.002)
        time.sleep(args.warmup_delay)
        for i in range(40):
            packet = build_request(i) + b"".join([
                _string(F_HOSTNAME, marker),
                _string(F_SERVER_NAME, marker),
                _string(F_SCRIPT_NAME, str(Path("/row_position") / str(i))),
                _float(F_REQUEST_TIME, 0.1 + i * 0.013),
            ])
            sock.sendto(packet, (args.udp_host, args.udp_port))
            time.sleep(0.002)

    predicate = f"server_name='{marker}'"
    deadline = time.monotonic() + 5
    while True:
        snapshot = query(f"SELECT id, req_time, mem_peak_usage, ru_utime, doc_size FROM request WHERE {predicate}")
        if len(snapshot) == 40:
            break
        if time.monotonic() >= deadline:
            raise RuntimeError(f"expected 40 collected requests, got {len(snapshot)}")
        time.sleep(0.05)

    for column, field in enumerate(["req_time", "mem_peak_usage", "ru_utime", "doc_size"], 1):
        for direction in ["ASC", "DESC"]:
            expected = sorted(snapshot, key=lambda row: (Decimal(row[column]), int(row[0])), reverse=direction == "DESC")
            for offset in [0, 1, 5, 10]:
                sql = (f"SELECT id, req_time, mem_peak_usage, ru_utime, doc_size FROM request WHERE {predicate} "
                       f"ORDER BY {field} {direction}, id {direction} LIMIT {offset}, 5")
                actual = query(sql, row_references=True)
                if actual != expected[offset:offset + 5]:
                    raise AssertionError(f"{field} {direction}, offset {offset}: {actual} != {expected[offset:offset + 5]}")
    print("PASS: request row references, four metrics, ASC/DESC and LIMIT offsets")

    ids = ",".join(str(int(row[0])) for row in snapshot)
    timer_sql = f"SELECT id, value, request_id FROM timer WHERE request_id IN ({ids})"
    timers = query(timer_sql)
    if len(timers) != 40:
        raise AssertionError(f"expected 40 timers, got {len(timers)}")
    expected_timers = sorted(timers, key=lambda row: (Decimal(row[1]), int(row[0])), reverse=True)
    actual_timers = query(timer_sql + " ORDER BY value DESC, id DESC LIMIT 5, 10", row_references=True)
    if actual_timers != expected_timers[5:15]:
        raise AssertionError(f"timer row references: {actual_timers} != {expected_timers[5:15]}")
    print("PASS: timer row references after request-id lookup")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AssertionError, RuntimeError) as error:
        sys.exit(f"FAIL: {error}")
