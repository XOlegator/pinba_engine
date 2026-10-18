#!/usr/bin/env python3
"""Verify receive/admission accounting and native-width status on a disposable DB."""

import argparse
from pathlib import Path
import socket
import subprocess
import time
import uuid

from send_pinba_packet import (
    build_request, _uint32, _string, _tag, _varint, F_HOSTNAME, F_SCRIPT_NAME,
    F_TIMER_HIT_COUNT, F_TIMER_TAG_COUNT, F_TIMER_TAG_VALUE, F_TAG_VALUE,
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mysql-client', default='mysql')
    parser.add_argument('--mysql-option', action='append', default=[])
    parser.add_argument('--database', default='pinba')
    parser.add_argument('--udp-host', default='127.0.0.1')
    parser.add_argument('--udp-port', type=int, default=30002)
    parser.add_argument('--failure-file', type=Path)
    args = parser.parse_args()
    client = [args.mysql_client, *args.mysql_option, '--batch', '--skip-column-names', args.database]

    def query(sql):
        result = subprocess.run(client, input=sql, text=True, capture_output=True, timeout=10)
        assert result.returncode == 0, result.stderr
        return [line.split('\t') for line in result.stdout.splitlines()]

    def snapshot():
        return {name.lower().removeprefix('pinba_'): int(value)
                for name, value in query("SHOW GLOBAL STATUS LIKE 'Pinba%'") if value.isdecimal()}

    def wait(predicate):
        deadline = time.monotonic() + 5
        while not predicate():
            assert time.monotonic() < deadline, snapshot()
            time.sleep(.03)

    assert int(query('SELECT @@global.pinba_stats_history')[0][0]) <= 10
    wait(lambda: snapshot()['queued_datagrams_live'] == 0)
    before = snapshot()
    marker = 'm' + uuid.uuid4().hex[:16]

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        def send(data):
            sock.sendto(data, (args.udp_host, args.udp_port))
            time.sleep(.002)

        packets = [build_request(i) for i in range(32)]
        base = build_request(1)
        suffix = _uint32(F_TAG_VALUE, 1)
        assert base.endswith(suffix)
        packets += [
            base + _uint32(F_TIMER_TAG_COUNT, 0),
            base + _uint32(F_TIMER_TAG_VALUE, 1),
            base[:-len(suffix)] + _uint32(F_TAG_VALUE, 999999),
            base.replace(_uint32(F_TIMER_HIT_COUNT, 1), _uint32(F_TIMER_HIT_COUNT, 0xffffffff)),
        ]
        packets += [base + _tag(18, 2) + _varint(len(base)) + base, b'', b'bad protobuf', b'\xff']
        for packet in packets:
            send(packet)
        wait(lambda: snapshot()['udp_datagrams_received'] >= before['udp_datagrams_received'] + len(packets))
        wait(lambda: snapshot()['queued_datagrams_live'] == 0)
        after = snapshot()
        delta = {key: after[key] - before[key] for key in before}
        assert delta['udp_datagrams_received'] == 40, delta
        assert delta['udp_bytes_received'] == sum(map(len, packets)), delta
        assert delta['udp_decode_errors'] == 3 and delta['udp_datagrams_enqueued'] == 37, delta
        assert delta['udp_queue_drops'] == 0 and delta['udp_datagrams_completed'] == 37, delta
        assert delta['requests_checked'] == 38 and delta['requests_invalid'] == 4, delta
        for key in ['invalid_timer_arrays', 'invalid_flattened_tags',
                    'invalid_dictionary_index', 'invalid_representation']:
            assert delta[key] == 1, (key, delta)
        assert delta['requests_validated'] == delta['requests_staged'] == delta['requests_admitted'] == 34, delta
        assert after['healthy'] == 1 and after['staging_capacity_slots'] > 0, after
        assert int(query('SELECT current_temp_pool_size FROM status')[0][0]) == min(
            after['staging_capacity_slots'], 2147483647)
        assert after['dictionary_words_peak'] >= after['dictionary_words_live'], after
        assert after['udp_receive_buffer_bytes'] > 0 and delta['udp_decode_ns'] > 0, after

        if args.failure_file:
            wait(lambda: snapshot()['queued_datagrams_live'] == 0)
            before = snapshot()
            try:
                args.failure_file.write_text('report_job_buffers')
                for _ in range(4):
                    send(base + _string(F_HOSTNAME, marker + '_jobs'))
                wait(lambda: snapshot()['requests_allocation_dropped'] >=
                     before['requests_allocation_dropped'] + 4)
                wait(lambda: snapshot()['queued_datagrams_live'] == 0)
                assert query(f"SELECT COUNT(*) FROM request WHERE hostname='{marker}_jobs'") == [['0']]
                args.failure_file.write_text('')
                before = snapshot()
                args.failure_file.write_text('raw_copy_buffer')
                for i in range(10):
                    root = base + _string(F_HOSTNAME, marker)
                    bad = root + _string(F_SCRIPT_NAME, marker + '_copyfail')
                    good = root + _string(F_SCRIPT_NAME, marker + '_good')
                    for child in [bad, good]:
                        root += _tag(18, 2) + _varint(len(child)) + child
                    send(root)
                wait(lambda: snapshot()['requests_allocation_dropped'] >=
                     before['requests_allocation_dropped'] + 10)
                wait(lambda: snapshot()['queued_datagrams_live'] == 0)
                assert query(f"SELECT COUNT(*) FROM request WHERE hostname='{marker}'") == [['20']]
                assert query(f"SELECT COUNT(*) FROM timer t JOIN request r ON r.id=t.request_id "
                             f"WHERE r.hostname='{marker}'") == [['20']]
                assert query(f"SELECT COUNT(*) FROM request WHERE script_name='{marker}_copyfail'") == [['0']]
                delta = {key: snapshot()[key] - before[key] for key in [
                    'requests_staged', 'requests_admitted', 'requests_allocation_dropped']}
                assert delta['requests_staged'] == 30 and delta['requests_admitted'] == 20, delta
                args.failure_file.write_text('expiration_job_buffers')
                before = snapshot()
                history = int(query('SELECT @@global.pinba_stats_history')[0][0])
                time.sleep(history + .2)
                assert query(f"SELECT COUNT(*) FROM request WHERE hostname='{marker}'") == [['20']]
                assert snapshot()['expiration_deferred_batches'] > before['expiration_deferred_batches']
                args.failure_file.write_text('')
                wait(lambda: query(f"SELECT COUNT(*) FROM request WHERE hostname='{marker}'") == [['0']])
            finally:
                args.failure_file.write_text('')
        print('PASS: datagram/Request accounting, reject reasons, gauges and mixed raw-allocation failure')


if __name__ == '__main__':
    main()
