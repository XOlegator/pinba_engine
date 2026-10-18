#!/usr/bin/env python3
"""Check concurrent DDL, query cancellation and failed/repeated INSTALL on a disposable DB."""

import argparse
from pathlib import Path
import socket
import subprocess
import threading
import time
import uuid

from send_pinba_packet import build_request


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mysql-client', default='mysql')
    parser.add_argument('--mysql-option', action='append', default=[])
    parser.add_argument('--database', default='pinba')
    parser.add_argument('--udp-host', default='127.0.0.1')
    parser.add_argument('--udp-port', type=int, default=30002)
    parser.add_argument('--server-pid', type=int)
    parser.add_argument('--failure-file', type=Path,
                        help='requires test-failure build and matching PINBA_INIT_FAILURE_FILE in server env')
    args = parser.parse_args()
    client = [args.mysql_client, *args.mysql_option, '--batch', '--skip-column-names', args.database]

    def run(sql, success=True):
        result = subprocess.run(client, input=sql, text=True, capture_output=True, timeout=20)
        if (result.returncode == 0) != success:
            raise AssertionError((sql, result.returncode, result.stderr))
        return [line.split('\t') for line in result.stdout.splitlines()]

    def scalar(sql):
        return int(run(sql)[0][0])

    if scalar('SELECT @@global.pinba_stats_history') > 10:
        raise RuntimeError('requires a disposable server with history <= 10')

    marker = '_lifecycle_' + uuid.uuid4().hex[:12]
    first, alias, renamed = marker + 'a', marker + 'b', marker + 'c'
    stop = threading.Event()
    failures = []

    def sender():
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            i = 0
            while not stop.is_set():
                sock.sendto(build_request(i), (args.udp_host, args.udp_port))
                i += 1
                time.sleep(0.003)

    def reader():
        try:
            while not stop.is_set():
                run('SELECT COUNT(*) FROM request; SELECT * FROM info')
        except Exception as error:
            failures.append(error)

    threads = [threading.Thread(target=sender), threading.Thread(target=reader)]
    try:
        for thread in threads:
            thread.start()
        run(f"CREATE TABLE {first} (req_count INT) ENGINE=PINBA COMMENT='info::tag.group=A'; "
            f"CREATE TABLE {alias} (req_count INT) ENGINE=PINBA COMMENT='info::tag.group=A'")
        for _ in range(10):
            run(f'SELECT * FROM {first}; SELECT * FROM {alias}; '
                f'RENAME TABLE {alias} TO {renamed}; SELECT * FROM {renamed}; '
                f'RENAME TABLE {renamed} TO {alias}')
        run(f'DROP TABLE {first}')
        assert scalar(f'SELECT req_count FROM {alias}') > 0
        # PINBA intentionally rejects ALTER COMMENT. Rejection must preserve
        # the existing report/registration rather than partially applying DDL.
        run(f"ALTER TABLE {alias} COMMENT='info::tag.group=B'", success=False)
        assert scalar(f'SELECT req_count FROM {alias}') > 0

        # The server cancels a real statement with an active PINBA handler.
        process = subprocess.Popen([*client, '--unbuffered'], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        process.stdin.write('SELECT CONNECTION_ID(); SELECT SLEEP(5) FROM info;\n')
        process.stdin.close()
        process.stdin = None
        connection = int(process.stdout.readline().strip())
        time.sleep(0.1)
        run(f'KILL QUERY {connection}')
        _, error = process.communicate(timeout=10)
        assert process.returncode != 0 and 'interrupt' in error.lower(), error
        run('SELECT * FROM info')
    finally:
        stop.set()
        for thread in threads:
            thread.join(timeout=20)
        run(f'DROP TABLE IF EXISTS {first}, {alias}, {renamed}')
    if failures:
        raise failures[0]

    def uninstall():
        run('FLUSH TABLES; UNINSTALL PLUGIN pinba')

    def install(success=True):
        run("INSTALL PLUGIN pinba SONAME 'ha_pinba.so'", success=success)

    uninstall()

    def resources():
        if args.server_pid is None:
            return None
        root = Path('/proc') / str(args.server_pid)
        return len(list((root / 'fd').iterdir())), len(list((root / 'task').iterdir()))

    baseline = resources()

    def check_cleanup():
        run('SELECT 1')
        if baseline is not None:
            deadline = time.monotonic() + 3
            while any(a > b + 1 for a, b in zip(resources(), baseline)):
                if time.monotonic() > deadline:
                    raise AssertionError(('leaked fd/thread after init failure', baseline, resources()))
                time.sleep(0.05)

    try:
        if args.failure_file:
            for stage in ['request_pool', 'timer_pool', 'thread_pool', 'staging_pools',
                          'socket', 'collector_thread', 'data_thread', 'stats_thread',
                          'data_buffers', 'stats_buffers']:
                args.failure_file.write_text(stage)
                install(success=False)
                check_cleanup()
                args.failure_file.write_text('')
                install()
                assert scalar('SELECT COUNT(*) FROM request') == 0
                uninstall()
                check_cleanup()
                print('PASS: failed init cleanup and retry:', stage)

        # A real bind failure must release pools/workers and allow retry in the
        # same server, independently of the optional test-only failure hooks.
        for _ in range(3):
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as occupied:
                occupied.bind((args.udp_host, args.udp_port))
                install(success=False)
                check_cleanup()
            install()
            uninstall()
            check_cleanup()
    finally:
        if args.failure_file:
            args.failure_file.write_text('')
        install()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.sendto(build_request(1), (args.udp_host, args.udp_port))
    deadline = time.monotonic() + 5
    while scalar('SELECT COUNT(*) FROM request') != 1:
        assert time.monotonic() < deadline, 'collector not working after reinstallation'
        time.sleep(0.05)
    print('PASS: concurrent SELECT/RENAME/ALTER/DROP, cancellation, bind failure, reinstall and resource cleanup')


if __name__ == '__main__':
    main()
