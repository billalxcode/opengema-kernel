#!/usr/bin/env python3
"""Serial test client for QEMU.

Connects to the QEMU unix serial socket, mirrors output to a log, drives the
gemabox shell through help, clear, and exit, and stops at halt.

Usage: uart_test_client.py <socket> <log> [timeout_seconds] [gate_dir]
Exit nonzero on connect timeout or missing halt marker. When gate_dir is set,
the client waits for gate_dir/clear-continue after writing
gate_dir/clear-ready, so the caller can inspect the framebuffer.
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
    gate_dir = argv[4] if len(argv) > 4 else None
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
    help_sent = False
    help_seen = False
    last_data = time.time()
    clear_sent = False
    clear_ready = False
    clear_released = False
    ls_sent = False
    ls_error_sent = False
    spawn_error_sent = False
    hello_sent = False
    exit_sent = False
    halted = False

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
                last_data = time.time()

            if not help_sent and b"hello from applications" in buffer:
                client.sendall(b"help\n")
                help_sent = True
                buffer.clear()

            if (help_sent and not help_seen and
                    b"commands: help, echo, cat, ls, clear, exit" in buffer):
                help_seen = True

            if (help_seen and not clear_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                client.sendall(b"clear\n")
                clear_sent = True
                buffer.clear()

            if clear_sent and not clear_ready and b"gemabox> " in buffer:
                if gate_dir is not None:
                    open(os.path.join(gate_dir, "clear-ready"), "w").close()
                clear_ready = True

            if clear_ready and not clear_released:
                if gate_dir is None or os.path.exists(
                        os.path.join(gate_dir, "clear-continue")):
                    clear_released = True

            if clear_released and not ls_sent:
                client.sendall(b"ls /\n")
                ls_sent = True

            if ls_sent and not ls_error_sent and b"system/" in buffer:
                client.sendall(b"ls /missing\n")
                ls_error_sent = True

            if ls_error_sent and not spawn_error_sent and b"ls: list failed" in buffer:
                client.sendall(b".\n")
                spawn_error_sent = True

            if spawn_error_sent and not hello_sent and b"run: spawn failed" in buffer:
                client.sendall(b"hello\n")
                hello_sent = True

            if hello_sent and not exit_sent and b"hello from applications" in buffer:
                client.sendall(b"exit\n")
                exit_sent = True

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
