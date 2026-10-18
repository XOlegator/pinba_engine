#!/usr/bin/env python3
"""Check opt-in fractional ranks and legacy COMMENT compatibility through UDP/SQL."""

import argparse
import socket
import subprocess
import time
import uuid

from send_pinba_packet import (
    F_HOSTNAME, F_SERVER_NAME, F_SCRIPT_NAME, F_REQUEST_COUNT, F_DOCUMENT_SIZE,
    F_MEMORY_PEAK, F_REQUEST_TIME, F_RU_UTIME, F_RU_STIME, F_TIMER_HIT_COUNT,
    F_TIMER_VALUE, F_TIMER_TAG_COUNT, F_TIMER_TAG_NAME, F_TIMER_TAG_VALUE,
    F_DICTIONARY, _string, _uint32, _float, _repeated_string,
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mysql-client', default='mysql')
    parser.add_argument('--mysql-option', action='append', default=[])
    parser.add_argument('--database', default='pinba')
    parser.add_argument('--udp-host', default='127.0.0.1')
    parser.add_argument('--udp-port', type=int, default=30002)
    args = parser.parse_args()
    client = [args.mysql_client, *args.mysql_option, '--batch', '--skip-column-names', args.database]

    def query(sql, success=True):
        result = subprocess.run(client, input=sql, text=True, capture_output=True, timeout=10)
        assert (result.returncode == 0) == success, (sql, result.stderr)
        return [line.split('\t') for line in result.stdout.splitlines()]

    def wait(predicate):
        deadline = time.monotonic() + 5
        while not predicate():
            assert time.monotonic() < deadline, 'percentile observations did not arrive'
            time.sleep(0.03)

    marker = 'p' + uuid.uuid4().hex[:16]
    fractional, legacy = '_' + marker + 'f', '_' + marker + 'l'
    columns = ('v VARCHAR(64), req_count INT, req_per_sec FLOAT, hit_count BIGINT, '
               'hit_per_sec FLOAT, timer_value FLOAT, timer_median FLOAT, '
               'ru_utime FLOAT, ru_stime FLOAT, index_value VARCHAR(256)')

    def packet(hits, value):
        return b''.join([
            _string(F_HOSTNAME, marker), _string(F_SERVER_NAME, marker),
            _string(F_SCRIPT_NAME, marker), _uint32(F_REQUEST_COUNT, 1),
            _uint32(F_DOCUMENT_SIZE, 1), _uint32(F_MEMORY_PEAK, 1),
            _float(F_REQUEST_TIME, .01), _float(F_RU_UTIME, 0), _float(F_RU_STIME, 0),
            _uint32(F_TIMER_HIT_COUNT, hits), _float(F_TIMER_VALUE, hits * value),
            _uint32(F_TIMER_TAG_COUNT, 1), _uint32(F_TIMER_TAG_NAME, 0),
            _uint32(F_TIMER_TAG_VALUE, 1), _repeated_string(F_DICTIONARY, [marker, 'value']),
        ])

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(packet(1, 0), (args.udp_host, args.udp_port))
        wait(lambda: bool(query(f"SELECT id FROM tag WHERE name='{marker}'")))
        try:
            query(f"CREATE TABLE {fractional} ({columns}, a FLOAT, b FLOAT, c FLOAT, d FLOAT) "
                  f"ENGINE=PINBA COMMENT='tag_info:{marker}::p=99.99,99.9,99.99,100'; "
                  f"CREATE TABLE {legacy} ({columns}, p FLOAT) "
                  f"ENGINE=PINBA COMMENT='tag_info:{marker}::99.9'; "
                  f"SELECT * FROM {fractional}; SELECT * FROM {legacy}")
            for hits, value in [(99000, 1), (900, 2), (90, 3), (10, 4)]:
                sock.sendto(packet(hits, value), (args.udp_host, args.udp_port))
            wait(lambda: query(f'SELECT hit_count FROM {fractional}') == [['100000']])
            a, b, c, d = map(float, query(f'SELECT a,b,c,d FROM {fractional}')[0])
            old = float(query(f'SELECT p FROM {legacy}')[0][0])
            assert .9 < old < 1.1, old
            assert 1.9 < b < 2.1 and 2.9 < a < 3.1 and a == c and 3.9 < d < 4.1, (a, b, c, d)
            for suffix in ['p=', 'p=NaN', 'p=inf', 'p=99.9junk', 'p=99e0', 'p=0',
                           'p=100.01', 'p=99.9,', 'p=99.1234567890']:
                query(f"CREATE TABLE {fractional}_bad ({columns}, p FLOAT) "
                      f"ENGINE=PINBA COMMENT='tag_info:{marker}::{suffix}'", success=False)
            for maximum in ['0', '-1', 'NaN', 'inf', '0.5', '2147483648']:
                query(f"CREATE TABLE {fractional}_bad (n INT) ENGINE=PINBA "
                      f"COMMENT='info::histogram_max_time={maximum}'", success=False)
            print('PASS: exact weighted p99.9/p99.99, order/duplicates, legacy p99 and invalid configuration')
        finally:
            query(f'DROP TABLE IF EXISTS {fractional}, {legacy}, {fractional}_bad')


if __name__ == '__main__':
    main()
