#!/usr/bin/env python3
"""Serial test client for QEMU.

Connects to the QEMU unix serial socket, mirrors output to a log, injects
four bytes once the kernel reports UART readiness, and stops at halt.

Usage: uart_test_client.py <socket> <log> [timeout_seconds]
Exit nonzero on connect timeout or missing halt marker.
"""

import os
import socket
import sys
import time


def main(argv):
    if len(argv) < 3:
        print("usage: uart_test_client.py <socket> <log> [timeout]")
        return 2

    socket_path = argv[1]
    log_path = argv[2]
    timeout = float(argv[3]) if len(argv) > 3 else 120.0
    deadline = time.time() + timeout

    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    while True:
        try:
            client.connect(socket_path)
            break
        except OSError:
            if time.time() >= deadline:
                print("uart client: connect timeout")
                return 1
            time.sleep(0.2)

    client.settimeout(0.5)
    buffer = bytearray()
    injected = False
    halted = False

    with open(log_path, "wb") as log:
        while time.time() < deadline:
            try:
                chunk = client.recv(4096)
            except socket.timeout:
                continue
            if not chunk:
                break
            log.write(chunk)
            log.flush()
            buffer.extend(chunk)

            if not injected and b"uart: ready" in buffer:
                client.sendall(b"ping")
                injected = True

            if b"kernel: halt" in buffer:
                halted = True
                break

    client.close()
    if not halted:
        print("uart client: halt marker not seen")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
