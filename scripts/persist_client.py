#!/usr/bin/env python3
"""Drive one shell command over the QEMU serial socket.

Usage: persist_client.py <socket> <log> <command> <marker> [timeout]
Exit nonzero when the prompt, marker, or halt is missing.
"""

import socket
import sys
import time


def send_line(client, text):
    for value in text:
        client.sendall(bytes((value,)))
        time.sleep(0.01)


def main(argv):
    if len(argv) < 5 or len(argv) > 6:
        print("usage: persist_client.py <socket> <log> <command> <marker> [timeout]")
        return 2

    socket_path = argv[1]
    log_path = argv[2]
    command = (argv[3] + "\n").encode()
    marker = argv[4].encode()
    timeout = float(argv[5]) if len(argv) > 5 else 90.0
    deadline = time.time() + timeout

    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    while True:
        try:
            client.connect(socket_path)
            break
        except OSError:
            if time.time() >= deadline:
                print("persist client: connect timeout")
                return 1
            time.sleep(0.2)

    client.settimeout(0.5)
    buffer = bytearray()
    sent = False
    halted = False
    halt_sent_at = None

    with open(log_path, "wb") as log:
        while time.time() < deadline:
            try:
                chunk = client.recv(4096)
            except socket.timeout:
                chunk = None
            if chunk == b"":
                break
            if chunk:
                log.write(chunk)
                log.flush()
                buffer.extend(chunk)

            if not sent and b"gemabox> " in buffer:
                send_line(client, command)
                sent = True
                buffer.clear()

            if sent and marker in buffer:
                send_line(client, b"halt\n")
                sent = False
                halted = True
                halt_sent_at = time.time()

            if b"kernel: halt" in buffer:
                break
            if (halted and halt_sent_at is not None and
                    time.time() - halt_sent_at > 3.0):
                break

    client.close()
    if not halted:
        print("persist client: marker %r missing" % argv[4])
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
