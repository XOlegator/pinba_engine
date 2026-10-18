#!/usr/bin/env python3
"""Check real UDP ingestion and SQL histogram buckets on a disposable Pinba DB."""

import argparse
from decimal import Decimal
import socket
import subprocess
import time
import uuid

from send_pinba_packet import (
    F_DICTIONARY, F_REQUEST_TIME, F_SERVER_NAME, F_TAG_VALUE,
    _float, _string, _uint32, build_request,
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mysql-client", default="mysql")
    parser.add_argument("--mysql-option", action="append", default=[])
    parser.add_argument("--database", default="pinba")
    parser.add_argument("--udp-host", default="127.0.0.1")
    parser.add_argument("--udp-port", type=int, default=30002)
    args = parser.parse_args()
    client = [args.mysql_client, *args.mysql_option, "--batch", "--skip-column-names", args.database]

    def query(sql):
        result = subprocess.run(client, input=sql, text=True, capture_output=True, timeout=10)
        if result.returncode:
            raise RuntimeError(result.stderr.strip())
        return [line.split("\t") for line in result.stdout.splitlines()]

    marker = "hist_" + uuid.uuid4().hex
    table = "_" + marker
    # A request-tag filter isolates these observations from other test traffic.
    condition = f"tag.group={marker},histogram_max_time=10"
    try:
        query(f"""
            CREATE TABLE {table} (
                req_count INT, time_total FLOAT, ru_utime_total FLOAT, ru_stime_total FLOAT,
                time_interval INT, kbytes_total FLOAT, memory_footprint FLOAT,
                req_time_median FLOAT, p100 FLOAT
            ) ENGINE=PINBA COMMENT='info::{condition}:100';
            CREATE TABLE {table}_hv (
                index_value VARCHAR(256), bucket INT, time_value FLOAT, hits INT, percent FLOAT
            ) ENGINE=PINBA COMMENT='hv.info::{condition}';
            SELECT * FROM {table};
        """)
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            for i, value in enumerate([0.0, 5.0, 9.999999, 10.0, 10.000001]):
                packet = build_request(i)
                # The fixture ends with its sole request tag's value (dictionary
                # entry 1). Point it at our appended marker, dictionary entry 4.
                suffix = _uint32(F_TAG_VALUE, 1)
                assert packet.endswith(suffix)
                packet = packet[:-len(suffix)] + _uint32(F_TAG_VALUE, 4)
                packet += (_string(F_DICTIONARY, marker)
                           + _string(F_SERVER_NAME, marker) + _float(F_REQUEST_TIME, value))
                sock.sendto(packet, (args.udp_host, args.udp_port))
                time.sleep(0.01)
        deadline = time.monotonic() + 5
        while True:
            rows = query(f"SELECT req_count, p100 FROM {table}")
            if rows and int(rows[0][0]) == 5:
                break
            if time.monotonic() >= deadline:
                raise AssertionError(f"expected five observations, got {rows}")
            time.sleep(0.05)
        buckets = {int(row[0]): int(row[1]) for row in query(
            f"SELECT bucket, hits FROM {table}_hv WHERE hits > 0")}
        last = int(query("SELECT @@global.pinba_histogram_size")[0][0]) - 1
        assert buckets == {0: 1, (last + 1) // 2: 1, last: 3}, buckets
        assert Decimal(rows[0][1]) == Decimal(10), rows
        print("PASS: upper-edge histogram buckets and p100 through UDP/SQL")
    finally:
        query(f"DROP TABLE IF EXISTS {table}_hv, {table}")


if __name__ == "__main__":
    main()
