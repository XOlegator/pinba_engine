#!/usr/bin/env python3
"""Deterministic UDP/SQL workload for a DISPOSABLE server, with persistent SQL connections.

Install requirements-benchmark.txt in a virtual environment. Never target a live
collector: this tool creates temporary report tables and waits for raw expiration.
MYSQL_PWD supplies the password; JSON results are local artifacts, not source files.
"""

import argparse
import hashlib
from importlib.metadata import version
import json
import math
import os
from pathlib import Path
import platform
import socket
import threading
import time

import pymysql

from send_pinba_packet import (
    F_HOSTNAME, F_SERVER_NAME, F_SCRIPT_NAME, F_REQUEST_COUNT, F_DOCUMENT_SIZE,
    F_MEMORY_PEAK, F_REQUEST_TIME, F_RU_UTIME, F_RU_STIME, F_TIMER_HIT_COUNT,
    F_TIMER_VALUE, F_TIMER_TAG_COUNT, F_TIMER_TAG_NAME, F_TIMER_TAG_VALUE,
    F_DICTIONARY, F_TAG_NAME, F_TAG_VALUE, _string, _uint32, _float,
    _repeated_uint32, _repeated_float, _repeated_string, _tag, _varint,
)


CASES = {
    'low': (1, 1, 1, False, False),
    'values': (1, 1, 2, True, False),
    'names': (1, 1, 2, False, True),
    'nested': (3, 1, 2, False, False),
    'timers': (1, 10, 10, False, False),
    'reports': (1, 1, 100, False, False),
}


def percentiles(samples):
    values = sorted(samples)
    return {f'p{p}_ms': values[max(0, math.ceil(len(values) * p / 100) - 1)] * 1000
            for p in [50, 95, 99]} if values else {}


def fixture(case, window, count, seed):
    nodes, timers, _, unique_values, unique_names = CASES[case]
    marker = f'bm{seed}_{case}'
    packets, req_seconds, hits = [], 0., 0
    for i in range(count):
        children = []
        for depth in range(nodes):
            serial = i * nodes + depth
            value = f'v{window}_{serial}' if unique_values else f'v{serial % 8}'
            name = f'op{window}_{serial}' if unique_names else 'operation'
            request_time = (1 + serial % 8) / 64
            hit_counts = [1 + (serial + n) % 3 for n in range(timers)]
            req_seconds += request_time
            hits += sum(hit_counts)
            parts = [
                _string(F_HOSTNAME, marker), _string(F_SERVER_NAME, 'benchmark'),
                _string(F_SCRIPT_NAME, f'/{marker}/{serial}'),
                _uint32(F_REQUEST_COUNT, 1), _uint32(F_DOCUMENT_SIZE, 1024),
                _uint32(F_MEMORY_PEAK, 1024), _float(F_REQUEST_TIME, request_time),
                _float(F_RU_UTIME, 0), _float(F_RU_STIME, 0),
                _repeated_uint32(F_TIMER_HIT_COUNT, hit_counts),
                _repeated_float(F_TIMER_VALUE, [h * (1 + (serial + n) % 8) / 64
                                                for n, h in enumerate(hit_counts)]),
                _repeated_uint32(F_TIMER_TAG_COUNT, [2] * timers),
                _repeated_uint32(F_TIMER_TAG_NAME, [0, 4] * timers),
                _repeated_uint32(F_TIMER_TAG_VALUE, [1, 5] * timers),
                _repeated_string(F_DICTIONARY, ['category', value, 'bench', marker, name, 'read']),
                _uint32(F_TAG_NAME, 2), _uint32(F_TAG_VALUE, 3),
            ]
            children.append(b''.join(parts))
        root = children[0]
        for child in children[1:]:
            root += _tag(18, 2) + _varint(len(child)) + child
        packets.append(root)
    return marker, packets, {'requests': count * nodes, 'timers': count * nodes * timers,
                             'hits': hits, 'request_seconds': req_seconds}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--socket', required=True)
    parser.add_argument('--user', default='root')
    parser.add_argument('--database', default='pinba')
    parser.add_argument('--udp-host', default='127.0.0.1')
    parser.add_argument('--udp-port', type=int, default=30002)
    parser.add_argument('--server-pid', type=int, required=True)
    parser.add_argument('--label', required=True, help='Commit, build flags and mode under test')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cases', default=','.join(CASES))
    parser.add_argument('--windows', type=int, default=3)
    parser.add_argument('--packets', type=int, default=300)
    parser.add_argument('--rate', type=float, default=2000)
    parser.add_argument('--seed', type=int, default=42)
    args = parser.parse_args()
    cases = args.cases.split(',')
    assert set(cases) <= CASES.keys() and args.windows > 0 and args.packets > 0 and args.rate > 0

    def connect():
        return pymysql.connect(unix_socket=args.socket, user=args.user,
                               password=os.environ.get('MYSQL_PWD', ''), database=args.database,
                               autocommit=True, read_timeout=15, write_timeout=15)

    def query(connection, sql, values=None):
        with connection.cursor() as cursor:
            cursor.execute(sql, values)
            return cursor.fetchall()

    connection = connect()

    def metrics():
        return {name.removeprefix('Pinba_'): int(value)
                for name, value in query(connection, "SHOW GLOBAL STATUS LIKE 'Pinba%'")
                if value.isdecimal()}

    def process_sample():
        base = Path('/proc') / str(args.server_pid)
        stat = (base / 'stat').read_text().rsplit(')', 1)[1].split()
        status = (base / 'status').read_text().splitlines()
        rss = next(int(line.split()[1]) * 1024 for line in status if line.startswith('VmRSS:'))
        return {'cpu_seconds': (int(stat[11]) + int(stat[12])) / os.sysconf('SC_CLK_TCK'),
                'rss_bytes': rss}

    history = int(query(connection, 'SELECT @@global.pinba_stats_history')[0][0])
    assert history <= 10, 'use a disposable server with history <= 10'
    assert args.packets / args.rate < history / 2, 'sending must finish before raw expiration'
    assert args.packets * max(CASES[c][0] for c in cases) < int(
        query(connection, 'SELECT @@global.pinba_request_pool_size')[0][0]), 'raw ring too small'
    settings = dict(query(connection, "SHOW GLOBAL VARIABLES LIKE 'pinba%'"))
    results = {'label': args.label, 'platform': platform.platform(),
               'machine': platform.machine(),
               'cgroup_limits': {name: (Path('/sys/fs/cgroup') / name).read_text().strip()
                                  for name in ['cpu.max', 'memory.max']
                                  if (Path('/sys/fs/cgroup') / name).exists()},
               'cpu_model': next((line.split(':', 1)[1].strip()
                   for line in Path('/proc/cpuinfo').read_text().splitlines()
                   if line.startswith('model name')), 'unknown'),
               'cpu_affinity_count': len(os.sched_getaffinity(args.server_pid)),
               'database_version': query(connection, 'SELECT VERSION()')[0][0],
               'driver_version': version('PyMySQL'), 'settings': settings,
               'parameters': {k: str(v) if isinstance(v, Path) else v for k, v in vars(args).items()
                              if k not in ['socket', 'user']}, 'cases': []}
    args.output.parent.mkdir(parents=True, exist_ok=True)

    def wait_empty():
        deadline = time.monotonic() + history + 5
        while int(query(connection, 'SELECT COUNT(*) FROM request')[0][0]):
            assert time.monotonic() < deadline, 'requests did not expire'
            time.sleep(.05)
        time.sleep(1.2)  # include the periodic dictionary/tag collection pass

    try:
        wait_empty()
        for case in cases:
            nodes, timers, report_count, _, _ = CASES[case]
            # Keep category alive until tag_info construction acquires its selector.
            _, seed_packets, _ = fixture('low', 0, 1, args.seed + 1)
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                sock.sendto(seed_packets[0], (args.udp_host, args.udp_port))
            time.sleep(.1)
            tables = []
            marker, _, _ = fixture(case, 0, 1, args.seed)
            try:
                # Each COMMENT has a distinct condition; aliases would share one underlying report.
                for number in range(report_count):
                    name = f'_bm_{case}_{number}'
                    query(connection, f'DROP TABLE IF EXISTS {name}')
                    condition = f'min_time={number * .000001:.6f},tag.bench={marker}'
                    if number % 2 == 0:
                        columns, comment = 'req_count BIGINT,time_total DOUBLE', f'info::{condition}'
                    else:
                        columns = 'value VARCHAR(64),req_count BIGINT,req_per_sec DOUBLE,hit_count BIGINT'
                        comment = f'tag_info:category:{condition}'
                    query(connection, f"CREATE TABLE {name} ({columns}) ENGINE=PINBA COMMENT='{comment}'")
                    tables.append((name, number % 2 == 0))
                    query(connection, f'SELECT * FROM {name}')
                case_result = {'name': case, 'nodes_per_datagram': nodes,
                               'timers_per_node': timers, 'reports': report_count, 'windows': []}
                for window in range(args.windows):
                    wait_empty()
                    marker, packets, oracle = fixture(case, window, args.packets, args.seed)
                    before, before_process = metrics(), process_sample()
                    latencies, visibility, sent_at, failures, samples = [], [], {}, [], []
                    stop = threading.Event()
                    measured = set()

                    def observer():
                        reader = None
                        try:
                            reader = connect()
                            iteration = 0
                            while not stop.is_set():
                                started = time.monotonic()
                                rows = query(reader, 'SELECT script_name FROM request WHERE hostname=%s', (marker,))
                                observed = time.monotonic()
                                for (script,) in rows:
                                    serial = int(script.rsplit('/', 1)[1])
                                    packet_id = serial // nodes
                                    if packet_id in sent_at and packet_id not in measured:
                                        visibility.append(observed - sent_at[packet_id])
                                        measured.add(packet_id)
                                name, info = tables[iteration % len(tables)]
                                query(reader, f'SELECT * FROM {name}')
                                latencies.append(time.monotonic() - started)
                                samples.append(process_sample())
                                iteration += 1
                                stop.wait(.01)
                        except Exception as error:
                            failures.append(repr(error))
                        finally:
                            if reader is not None:
                                reader.close()

                    thread = threading.Thread(target=observer, daemon=True)
                    thread.start()
                    try:
                        start = time.monotonic()
                        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                            for number, packet in enumerate(packets):
                                delay = start + number / args.rate - time.monotonic()
                                if delay > 0:
                                    time.sleep(delay)
                                sent_at[number] = time.monotonic()
                                sock.sendto(packet, (args.udp_host, args.udp_port))
                        sent_seconds = time.monotonic() - start
                        deadline = start + history - .25
                        while True:
                            raw = int(query(connection, 'SELECT COUNT(*) FROM request WHERE hostname=%s', (marker,))[0][0])
                            if raw == oracle['requests']:
                                break
                            assert time.monotonic() < deadline, (case, window, 'ingest loss', raw, oracle, metrics())
                            time.sleep(.01)
                        visible_seconds = time.monotonic() - start
                        # Validate every distinct report, not just the one used for timed reads.
                        for name, info in tables:
                            if info:
                                count, seconds = query(connection, f'SELECT req_count,time_total FROM {name}')[0]
                                assert int(count) == oracle['requests'], (name, count, oracle)
                                assert math.isclose(float(seconds), oracle['request_seconds'], rel_tol=2e-5, abs_tol=.00101), (name, seconds, oracle)
                            else:
                                count, hits = query(connection, f'SELECT SUM(req_count),SUM(hit_count) FROM {name}')[0]
                                assert int(count) == oracle['requests'] and int(hits) == oracle['hits'], (name, count, hits, oracle)
                        assert int(query(connection, 'SELECT COUNT(*) FROM timer t JOIN request r ON r.id=t.request_id '
                                                    'WHERE r.hostname=%s', (marker,))[0][0]) == oracle['timers']
                        during = metrics()
                        wait_empty()  # Reader remains active through subtraction and collection.
                        for name, info in tables:
                            if info:
                                assert int(query(connection, f'SELECT req_count FROM {name}')[0][0]) == 0
                            else:
                                assert not query(connection, f'SELECT * FROM {name}')
                    finally:
                        stop.set()
                        thread.join(timeout=20)
                    assert not thread.is_alive() and not failures, failures
                    assert latencies and len(visibility) == args.packets, (len(latencies), len(visibility))
                    after, after_process = metrics(), process_sample()
                    delta = {key: after[key] - before[key] for key in before}
                    if 'requests_admitted' in delta:
                        assert delta['requests_admitted'] == oracle['requests'], delta
                        assert delta['udp_datagrams_received'] == args.packets, delta
                        assert delta['requests_resource_rejected'] == delta['udp_queue_drops'] == delta['udp_kernel_drops'] == 0, delta
                    result = {'fixture_sha256': hashlib.sha256(b''.join(packets)).hexdigest(),
                              'oracle': oracle, 'udp_payload_bytes': sum(map(len, packets)),
                              'datagram_bytes_min': min(map(len, packets)),
                              'datagram_bytes_max': max(map(len, packets)),
                              'sent_seconds': sent_seconds,
                              'observed_complete_seconds': visible_seconds,
                              'offered_datagrams_per_second': args.packets / sent_seconds,
                              'sql_raw_plus_report_latency': percentiles(latencies),
                              'sql_samples': len(latencies),
                              'visibility_poll_upper_bound': percentiles(visibility),
                              'visibility_samples': len(visibility),
                              'server_cpu_seconds': after_process['cpu_seconds'] - before_process['cpu_seconds'],
                              'rss_before_bytes': before_process['rss_bytes'],
                              'rss_peak_bytes': max([before_process['rss_bytes']] + [s['rss_bytes'] for s in samples]),
                              'rss_after_bytes': after_process['rss_bytes'],
                              'metrics_live': during, 'metrics_quiet': after, 'metrics_delta': delta}
                    case_result['windows'].append(result)
                    print(json.dumps({'case': case, 'window': window, 'cpu': result['server_cpu_seconds'],
                                      'sql': result['sql_raw_plus_report_latency'],
                                      'rss_after': result['rss_after_bytes']}), flush=True)
                results['cases'].append(case_result)
                args.output.write_text(json.dumps(results, indent=2) + '\n')
            finally:
                for name, _ in tables:
                    query(connection, f'DROP TABLE IF EXISTS {name}')
    finally:
        connection.close()


if __name__ == '__main__':
    main()
