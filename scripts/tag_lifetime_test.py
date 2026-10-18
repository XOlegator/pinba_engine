#!/usr/bin/env python3
"""Verify opt-in tag GC, cursor/report ownership and admission limits on a disposable DB."""

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

    def query(sql):
        result = subprocess.run(client, input=sql, text=True, capture_output=True, timeout=20)
        assert result.returncode == 0, result.stderr
        return [line.split('\t') for line in result.stdout.splitlines()]

    def scalar(sql):
        return int(query(sql)[0][0])

    def metric(name):
        return int(query(f"SHOW GLOBAL STATUS LIKE 'Pinba_{name}'")[0][1])

    history = scalar('SELECT @@global.pinba_stats_history')
    assert history <= 10, 'use a disposable server with history <= 10'
    gc = scalar('SELECT @@global.pinba_tag_gc_enabled')
    limits = [scalar('SELECT @@global.pinba_' + name) for name in [
        'ingest_dictionary_words_limit', 'ingest_dictionary_bytes_limit', 'tag_names_limit']]
    marker = 'g' + uuid.uuid4().hex[:12]

    def wait(predicate):
        deadline = time.monotonic() + history + 6
        while not predicate():
            assert time.monotonic() < deadline, 'condition did not converge'
            time.sleep(.04)

    def packet(name, value='v'):
        return b''.join([
            _string(F_HOSTNAME, marker), _string(F_SERVER_NAME, marker),
            _string(F_SCRIPT_NAME, marker), _uint32(F_REQUEST_COUNT, 1),
            _uint32(F_DOCUMENT_SIZE, 1024), _uint32(F_MEMORY_PEAK, 1024),
            _float(F_REQUEST_TIME, .125), _float(F_RU_UTIME, 0), _float(F_RU_STIME, 0),
            _uint32(F_TIMER_HIT_COUNT, 1), _float(F_TIMER_VALUE, .0625),
            _uint32(F_TIMER_TAG_COUNT, 1), _uint32(F_TIMER_TAG_NAME, 0),
            _uint32(F_TIMER_TAG_VALUE, 1), _repeated_string(F_DICTIONARY, [name, value]),
        ])

    def live(name):
        return query(f"SELECT id FROM tag WHERE name='{name}'")

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        def send(name, value='v'):
            sock.sendto(packet(name, value), (args.udp_host, args.udp_port))
            time.sleep(.003)

        if any(limits):
            before = metric('requests_admitted')
            rejected = metric('requests_resource_rejected')
            names = [marker + str(i) for i in range(24)]
            for name in names:
                send(name)
            wait(lambda: metric('requests_checked') >= 24)
            wait(lambda: metric('queued_datagrams_live') == 0)
            accepted = metric('requests_admitted') - before
            assert 0 < accepted < 24, accepted
            assert metric('requests_resource_rejected') - rejected == 24 - accepted
            assert scalar(f"SELECT COUNT(*) FROM request WHERE hostname='{marker}'") == accepted
            assert scalar(f"SELECT COUNT(*) FROM timer t JOIN request r ON r.id=t.request_id "
                          f"WHERE r.hostname='{marker}'") == accepted
            if limits[0]:
                assert metric('dictionary_words_live') <= limits[0]
            if limits[1]:
                assert metric('dictionary_bytes_known') <= limits[1]
            if limits[2]:
                assert metric('tag_descriptors_live') <= limits[2]
                assert metric('tag_limit_rejected') > 0
            else:
                assert metric('dictionary_limit_rejected') > 0
            wait(lambda: scalar('SELECT COUNT(*) FROM request') == 0)
            if gc:
                wait(lambda: metric('tag_descriptors_live') == 0)
                wait(lambda: metric('dictionary_words_live') == 0)
                send(marker + 'recovered')
                wait(lambda: metric('requests_admitted') > before + accepted)
            print('PASS: whole-Request admission limits and recovery after expiration')
            return

        wait(lambda: scalar('SELECT COUNT(*) FROM request') == 0)
        time.sleep(1.2)
        baseline = metric('tag_descriptors_live')
        previous_max = -1
        for window in range(3):
            names = [marker + f'w{window}n{i}' for i in range(40)]
            before = metric('requests_admitted')
            for name in names:
                send(name)
            wait(lambda: metric('requests_admitted') >= before + len(names))
            rows = query(f"SELECT id,name FROM tag WHERE name LIKE '{marker}%' ORDER BY id")
            ids = [int(row[0]) for row in rows]
            assert ids == sorted(ids) and len(ids) == len(set(ids)), rows
            current = [int(live(name)[0][0]) for name in names]
            assert min(current) > previous_max
            previous_max = max(current)
            for column in ['id', 'name']:
                expected = sorted(rows, key=lambda r: int(r[0]) if column == 'id' else r[1])
                for direction in ['ASC', 'DESC']:
                    oracle = expected if direction == 'ASC' else expected[::-1]
                    actual = query(f"SELECT id,name FROM tag WHERE name LIKE '{marker}%' "
                                   f"ORDER BY {column} {direction} LIMIT 3,11")
                    assert actual == oracle[3:14], (column, direction, actual, oracle)
            wait(lambda: scalar('SELECT COUNT(*) FROM request') == 0)
            if gc:
                wait(lambda: metric('tag_descriptors_live') == baseline)
                assert not live(names[0])
                assert not query(f'SELECT id FROM tag WHERE id={current[0]}')
            else:
                assert all(live(name) for name in names)
        if not gc:
            print('PASS: default persistent tag names/IDs across three expiration windows')
            return

        name = marker + 'pinned'
        send(name)
        wait(lambda: bool(live(name)))
        original_id = live(name)[0][0]
        table = '_' + marker
        query(f"CREATE TABLE {table} (value VARCHAR(64), req_count INT, req_per_sec FLOAT, "
              f"hit_count INT) ENGINE=PINBA COMMENT='tag_info:{name}'")
        try:
            query(f'SELECT * FROM {table}')
            wait(lambda: scalar('SELECT COUNT(*) FROM request') == 0)
            time.sleep(1.2)
            assert live(name) == [[original_id]], 'report selector must keep its descriptor'
            send(name)
            wait(lambda: scalar(f'SELECT COALESCE(SUM(hit_count),0) FROM {table}') == 1)
            assert live(name) == [[original_id]]
        finally:
            query(f'DROP TABLE IF EXISTS {table}')
        wait(lambda: not live(name))

        for cancel in [False, True]:
            name = marker + 'cursor' + str(int(cancel))
            send(name)
            send(name + 'other')  # Prevent the one-row SYSTEM/const-table optimization.
            wait(lambda: bool(live(name)) and bool(live(name + 'other')))
            sleeper = subprocess.Popen(client + ['-e', f"SELECT name,SLEEP({history + 4}) "
                                                f"FROM tag IGNORE INDEX (PRIMARY,name) WHERE name LIKE '{name}%'"],
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                wait(lambda: bool(query("SELECT ID FROM information_schema.PROCESSLIST "
                                        "WHERE STATE='User sleep'")))
                time.sleep(history + 1.2)
                assert live(name) or live(name + 'other'), 'in-flight SQL cursor must pin its descriptor'
                if cancel:
                    thread_id = query("SELECT ID FROM information_schema.PROCESSLIST "
                                      f"WHERE STATE='User sleep' AND INFO LIKE '%{name}%' ")[0][0]
                    query(f'KILL QUERY {int(thread_id)}')
                out, err = sleeper.communicate(timeout=2 * history + 12)
                if cancel:
                    assert sleeper.returncode != 0 and 'interrupt' in err.lower(), err
                else:
                    assert sleeper.returncode == 0 and name in out, err
            finally:
                if sleeper.poll() is None:
                    sleeper.kill()
                    sleeper.communicate()
            wait(lambda: not live(name) and not live(name + 'other'))
        print('PASS: tag GC with ID holes, sorted row references, report and SQL cursor pins')


if __name__ == '__main__':
    main()
