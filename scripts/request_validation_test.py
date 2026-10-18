#!/usr/bin/env python3
"""Exercise structural rejects, skipped timers and nested ownership on a disposable DB."""

import argparse
import socket
import subprocess
import time
import uuid

from send_pinba_packet import (
    F_HOSTNAME, F_SERVER_NAME, F_SCRIPT_NAME, F_REQUEST_COUNT, F_DOCUMENT_SIZE,
    F_MEMORY_PEAK, F_REQUEST_TIME, F_RU_UTIME, F_RU_STIME, F_TIMER_HIT_COUNT,
    F_TIMER_VALUE, F_TIMER_TAG_COUNT, F_TIMER_TAG_NAME, F_TIMER_TAG_VALUE,
    F_DICTIONARY, _string, _uint32, _float, _repeated_uint32,
    _repeated_float, _repeated_string, _tag, _varint,
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
        result = subprocess.run(client, input=sql, text=True, capture_output=True, timeout=10)
        if result.returncode:
            raise RuntimeError(result.stderr)
        return [line.split('\t') for line in result.stdout.splitlines()]

    def scalar(sql):
        return int(query(sql)[0][0])

    def wait(predicate):
        deadline = time.monotonic() + 5
        while not predicate():
            if time.monotonic() > deadline:
                raise AssertionError('UDP/SQL validation result did not become visible')
            time.sleep(0.03)

    marker = 'v' + uuid.uuid4().hex[:16]

    def packet(name, hits=(1,), counts=(1,), names=(0,), values=(2,), times=None, children=()):
        fields = [
            _string(F_HOSTNAME, marker), _string(F_SERVER_NAME, marker),
            _string(F_SCRIPT_NAME, name), _uint32(F_REQUEST_COUNT, 1),
            _uint32(F_DOCUMENT_SIZE, 1024), _uint32(F_MEMORY_PEAK, 1024),
            _float(F_REQUEST_TIME, 0.02), _float(F_RU_UTIME, 0), _float(F_RU_STIME, 0),
            _repeated_uint32(F_TIMER_HIT_COUNT, hits),
            _repeated_float(F_TIMER_VALUE, times if times is not None else [0.01] * len(hits)),
            _repeated_uint32(F_TIMER_TAG_COUNT, counts),
            _repeated_uint32(F_TIMER_TAG_NAME, names),
            _repeated_uint32(F_TIMER_TAG_VALUE, values),
            _repeated_string(F_DICTIONARY, [marker, 'skipped', 'correct']),
        ]
        for child in children:
            fields.append(_tag(18, 2) + _varint(len(child)) + child)
        return b''.join(fields)

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        def send(data):
            sock.sendto(data, (args.udp_host, args.udp_port))

        # The surviving timer must occupy the first stored slot and consume its
        # own input tags after the skipped timer's tag span.
        name = marker + '_skip'
        send(packet(name, hits=(0, 2), counts=(1, 1), names=(0, 0), values=(1, 2)))
        wait(lambda: scalar(f"SELECT COUNT(*) FROM request WHERE script_name='{name}'") == 1)
        rows = query(f"SELECT t.hit_count, tt.value FROM request r JOIN timer t "
                     f"ON t.request_id=r.id JOIN timertag tt ON tt.timer_id=t.id "
                     f"WHERE r.script_name='{name}'")
        assert rows == [['2', 'correct']], rows

        rejected_before = scalar('SELECT invalid_request_data FROM status')
        malformed = [
            dict(counts=(2,)), dict(names=(0, 0)), dict(values=()),
            dict(values=(3,)), dict(names=(0xffffffff,)), dict(counts=(0xffffffff,)),
            dict(hits=(0xffffffff,)), dict(times=()),
        ]
        for i, case in enumerate(malformed):
            send(packet(marker + '_bad' + str(i), **case))
        send(packet(marker + '_barrier'))
        wait(lambda: scalar('SELECT invalid_request_data FROM status') >=
             rejected_before + len(malformed))
        assert scalar(f"SELECT COUNT(*) FROM request WHERE script_name LIKE '{marker}\\_bad%'") == 0

        # Reject the parent independently; its valid child and grandchild must
        # stay allocated and must both be processed exactly once.
        grandchild = packet(marker + '_grandchild')
        child = packet(marker + '_child', children=(grandchild,))
        send(packet(marker + '_badparent', values=(9999,), children=(child,)))
        wait(lambda: scalar(f"SELECT COUNT(*) FROM request WHERE script_name IN "
                            f"('{marker}_child','{marker}_grandchild')") == 2)
        assert scalar(f"SELECT COUNT(*) FROM request WHERE script_name='{marker}_badparent'") == 0
        assert scalar(f"SELECT COUNT(*) FROM timer t JOIN request r ON r.id=t.request_id "
                      f"WHERE r.script_name IN ('{marker}_child','{marker}_grandchild')") == 2
        print('PASS: flattened arrays, indices, representation bounds, skipped timers and nested ownership')


if __name__ == '__main__':
    main()
