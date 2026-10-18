#!/usr/bin/env python3
"""Check dictionary ownership through real UDP/SQL and natural expiration.

Requires default_tables.sql on a DISPOSABLE server with stats_history <= 10.
No global data deletion or production restart is performed. MYSQL_PWD supplies
the password; --mysql-option can select a private socket or TCP endpoint.
"""

import argparse
import concurrent.futures
import socket
import subprocess
import threading
import time
import uuid

from send_pinba_packet import (
    F_DICTIONARY, F_HOSTNAME, F_REQUEST_COUNT, F_DOCUMENT_SIZE, F_MEMORY_PEAK,
    F_REQUEST_TIME, F_RU_UTIME, F_RU_STIME, F_SCRIPT_NAME, F_SERVER_NAME,
    F_TAG_NAME, F_TAG_VALUE, F_TIMER_HIT_COUNT, F_TIMER_VALUE, F_TIMER_TAG_COUNT,
    F_TIMER_TAG_NAME, F_TIMER_TAG_VALUE, _float, _string, _uint32,
    _repeated_string, _repeated_uint32, _tag, _varint,
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
        result = subprocess.run(client, input=sql, text=True, capture_output=True, timeout=15)
        if result.returncode:
            raise RuntimeError(result.stderr.strip())
        return [line.split("\t") for line in result.stdout.splitlines()]

    def scalar(sql):
        return int(query(sql)[0][0])

    history = scalar("SELECT @@global.pinba_stats_history")
    if history > 10:
        raise RuntimeError("use a disposable server with pinba_stats_history <= 10")

    def wait_for(predicate, description):
        deadline = time.monotonic() + history + 6
        while not predicate():
            if time.monotonic() > deadline:
                raise AssertionError(description)
            time.sleep(0.05)

    wait_for(lambda: scalar("SELECT COUNT(*) FROM request") == 0, "raw pool did not expire")
    # Collection can lag logical expiration by one second.
    time.sleep(1.1)
    baseline = scalar("SELECT dictionary_size FROM status")
    marker = "d" + uuid.uuid4().hex[:16]
    names = [marker + str(i) for i in range(3)]
    tables = []
    stop = threading.Event()
    failures = []

    def packet(value, *, timer_only=False, invalid=False):
        dictionary = [*names, value, "aux_" + value, "third_" + value, value]
        fields = [
            _string(F_HOSTNAME, marker), _string(F_SERVER_NAME, marker),
            _string(F_SCRIPT_NAME, "/" + marker), _uint32(F_REQUEST_COUNT, 1),
            _uint32(F_DOCUMENT_SIZE, 1024), _uint32(F_MEMORY_PEAK, 1024),
            _float(F_REQUEST_TIME, 0.02), _float(F_RU_UTIME, 0.01), _float(F_RU_STIME, 0),
            _uint32(F_TIMER_HIT_COUNT, 1), _float(F_TIMER_VALUE, 0.01),
            _uint32(F_TIMER_TAG_COUNT, 3),
            _repeated_uint32(F_TIMER_TAG_NAME, [0, 1, 2]),
            _repeated_uint32(F_TIMER_TAG_VALUE, [3, 4, 6]),
            _repeated_string(F_DICTIONARY, dictionary),
        ]
        if not timer_only:
            fields += [_repeated_uint32(F_TAG_NAME, [0, 1, 2]),
                       _repeated_uint32(F_TAG_VALUE, [999999 if invalid else 3, 4, 5])]
        return b"".join(fields)

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        def send(value, **kwargs):
            sock.sendto(packet(value, **kwargs), (args.udp_host, args.udp_port))
            time.sleep(0.001)

        def create_report(suffix, columns, comment):
            table = "_" + marker + suffix
            query(f"CREATE TABLE {table} ({columns}) ENGINE=PINBA COMMENT='{comment}'")
            tables.append(table)
            query(f"SELECT * FROM {table}")
            return table

        def reader():
            try:
                while not stop.is_set():
                    # Materialize strings, not just counts, while expiration and
                    # GC run. Each query has its own normal live-table semantics.
                    query("SELECT tags FROM request LIMIT 20; SELECT value FROM timertag LIMIT 20")
                    for table in tables[:6]:
                        query(f"SELECT * FROM {table} LIMIT 10")
            except Exception as error:
                failures.append(error)
                stop.set()

        def ddl_churn():
            table = "_" + marker + "_churn"
            try:
                for i in range(100):
                    if stop.is_set():
                        break
                    query(f"CREATE TABLE {table} (req_count INT) ENGINE=PINBA "
                          f"COMMENT='info::tag.{names[0]}=churn{i}'; SELECT * FROM {table}")
                    query(f"DROP TABLE {table}")
                    time.sleep(0.02)
            except Exception as error:
                failures.append(error)
                stop.set()
            finally:
                query(f"DROP TABLE IF EXISTS {table}")

        executor = concurrent.futures.ThreadPoolExecutor(max_workers=2)
        future = None
        try:
            send("seed")
            wait_for(lambda: scalar(f"SELECT COUNT(*) FROM request WHERE hostname='{marker}'") == 1,
                     "seed packet was not received")
            create_report("r1", "v VARCHAR(64), req_count INT", f"rtag_info:{names[0]}")
            create_report("r2", "v0 VARCHAR(64), v1 VARCHAR(64), req_count INT",
                          f"rtag2_info:{names[0]},{names[1]}")
            create_report("rn", "v0 VARCHAR(64), v1 VARCHAR(64), v2 VARCHAR(64), req_count INT",
                          "rtagN_info:" + ",".join(names))
            create_report("tn", "v0 VARCHAR(64), v1 VARCHAR(64), v2 VARCHAR(64), req_count INT",
                          "tagN_info:" + ",".join(names))
            condition_columns = ("req_count INT, time_total FLOAT, ru_utime FLOAT, ru_stime FLOAT, "
                                 "time_interval INT, kbytes_total FLOAT, memory_footprint FLOAT, "
                                 "req_time_median FLOAT")
            conditional = create_report("cond", condition_columns, f"info::tag.{names[0]}=keep")
            alias = create_report("alias", condition_columns, f"info::tag.{names[0]}=keep")
            # Two tables share the same report and selector ownership.
            assert scalar(f"SELECT COUNT(*) FROM {alias}") == 1
            future = executor.submit(reader)
            ddl_future = executor.submit(ddl_churn)

            for window in range(3):
                for i in range(400):
                    send(f"window{window}_{i}")
                wait_for(lambda: scalar(f"SELECT COUNT(*) FROM request WHERE hostname='{marker}'") >= 400,
                         "window traffic did not reach the raw pool")
                assert scalar("SELECT dictionary_size FROM status") >= baseline + 1000
                wait_for(lambda: scalar(f"SELECT COUNT(*) FROM request WHERE hostname='{marker}'") == 0,
                         "window raw requests did not expire")
                # Only configured report selectors/conditions may remain pinned;
                # unique values and staging caches must have been reclaimed.
                wait_for(lambda: scalar("SELECT dictionary_size FROM status") <= baseline + 4,
                         "expired dictionary values are still retained")
                if failures:
                    raise failures[0]
                print(f"PASS: window {window + 1}, unique values reclaimed with active reports")

            send("keep")
            wait_for(lambda: scalar(f"SELECT req_count FROM {conditional}") == 1,
                     "condition selector lost its identity after expiration")
            # Readers must stop before their tables are dropped.
            stop.set()
            future.result(timeout=20)
            ddl_future.result(timeout=20)
            if failures:
                raise failures[0]
            query(f"DROP TABLE {alias}")
            tables.remove(alias)
            assert scalar(f"SELECT req_count FROM {conditional}") == 1
            for table in list(tables):
                query(f"DROP TABLE {table}")
                tables.remove(table)
            wait_for(lambda: scalar("SELECT dictionary_size FROM status") == baseline,
                     "dropping reports did not release their selectors")

            # Timer-only packets use the parent dictionary without request tags.
            for i in range(20):
                send("timer_" + str(i), timer_only=True)
            wait_for(lambda: scalar(f"SELECT COUNT(*) FROM request WHERE hostname='{marker}'") == 20,
                     "timer-only requests were not received")
            assert scalar("SELECT COUNT(*) FROM timertag") >= 60
            wait_for(lambda: scalar("SELECT dictionary_size FROM status") == baseline,
                     "timer-only dictionary did not expire")

            # Semantic validation fails after interning: these temporary owners
            # must be released even though no raw request will be created.
            for i in range(20):
                send("invalid_" + str(i), invalid=True)
            send("validation_barrier")
            wait_for(lambda: scalar(f"SELECT COUNT(*) FROM request WHERE hostname='{marker}'") == 1,
                     "validation-barrier packet was not received")
            wait_for(lambda: scalar("SELECT dictionary_size FROM status") == baseline,
                     "rejected staging dictionaries are still retained")

            # A batch contains three Requests but only one datagram.
            parent = packet("parent")
            for value in ["child_one", "child_two"]:
                child = packet(value)
                parent += _tag(18, 2) + _varint(len(child)) + child
            sock.sendto(parent, (args.udp_host, args.udp_port))
            wait_for(lambda: scalar(f"SELECT COUNT(*) FROM request WHERE hostname='{marker}'") == 3,
                     "nested requests were not all copied")
            assert scalar("SELECT COUNT(*) FROM timertag") == 9
            wait_for(lambda: scalar("SELECT dictionary_size FROM status") == baseline,
                     "nested request dictionaries did not expire")

            # Also release staging owners rejected at a full raw request ring.
            capacity = scalar("SELECT @@global.pinba_request_pool_size")
            if capacity <= 2000:
                lost_before = scalar("SELECT lost_tmp_records FROM status")
                for i in range(capacity + 200):
                    sock.sendto(packet("overflow_" + str(i)), (args.udp_host, args.udp_port))
                    time.sleep(0.0002)
                wait_for(lambda: scalar("SELECT lost_tmp_records FROM status") > lost_before,
                         "test did not saturate the raw ring")
                wait_for(lambda: scalar("SELECT dictionary_size FROM status") == baseline,
                         "dropped staging dictionaries are still retained")
            print("PASS: aliases, raw wraparound, timer-only, rejected/nested and capacity drops")
        finally:
            stop.set()
            executor.shutdown(wait=True)
            for table in reversed(tables):
                query(f"DROP TABLE IF EXISTS {table}")


if __name__ == "__main__":
    main()
