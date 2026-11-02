#!/usr/bin/env python3
"""Serial test client for QEMU.

Connects to the QEMU unix serial socket, mirrors output to a log, drives the
gemabox shell through help, acpi, clear, and halt, and stops at halt.

Usage: uart_test_client.py <socket> <log> [timeout_seconds] [gate_dir]
Exit nonzero on connect timeout or missing halt marker. When gate_dir is set,
the client waits for gate_dir/clear-continue after writing
gate_dir/clear-ready, so the caller can inspect the framebuffer.
"""

import os
import socket
import sys
import time


def send_line(client, text):
    """Send a command slowly so the PL011 RX FIFO is always drained."""
    for value in text:
        client.sendall(bytes((value,)))
        time.sleep(0.01)


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
    acpi_sent = False
    acpi_seen = False
    time_sent = False
    time_seen = False
    ps_sent = False
    ps_seen = False
    dmesg_sent = False
    dmesg_seen = False
    level_sent = False
    level_seen = False
    fs_sent = False
    fs_seen = False
    clear_sent = False
    clear_ready = False
    clear_released = False
    ls_sent = False
    ls_error_sent = False
    spawn_error_sent = False
    hello_sent = False
    halt_sent = False
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

            if not help_sent and b"gemabox> " in buffer:
                send_line(client, b"help\n")
                help_sent = True
                buffer.clear()

            if (help_sent and not help_seen and
                    b"commands: help, echo, cat, ls, clear, acpi, uptime, sleep, ps, dmesg, loglevel, persist, mkdir, rm, mv, halt, shutdown, reboot" in buffer):
                help_seen = True

            if (help_seen and not acpi_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"acpi\n")
                acpi_sent = True
                buffer.clear()

            if acpi_sent and not acpi_seen and b"ACPI" in buffer:
                acpi_seen = True

            if (acpi_seen and not time_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"sleep 50\nuptime\n")
                time_sent = True
                buffer.clear()

            if time_sent and not time_seen and b"uptime: " in buffer:
                time_seen = True

            if (time_seen and not ps_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"ps\n")
                ps_sent = True
                buffer.clear()

            if ps_sent and not ps_seen and b"pid ppid" in buffer:
                ps_seen = True

            if (ps_seen and not dmesg_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"dmesg\n")
                dmesg_sent = True
                buffer.clear()

            if dmesg_sent and not dmesg_seen and b"dmesg:" in buffer:
                dmesg_seen = True

            if (dmesg_seen and not level_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"loglevel\nloglevel debug\nloglevel info\n")
                level_sent = True
                buffer.clear()

            if (level_sent and not level_seen and
                    b"loglevel: debug (3)" in buffer and
                    b"loglevel: info (2)" in buffer):
                level_seen = True

            if (level_seen and not fs_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"mkdir /tmp/d\nmv /tmp/d /tmp/e\nls /tmp\nrm /tmp/e\n")
                fs_sent = True
                buffer.clear()

            if fs_sent and not fs_seen and b"e/" in buffer:
                fs_seen = True

            if (fs_seen and not clear_sent and
                    time.time() - last_data > 0.5 and
                    b"gemabox> " in buffer):
                send_line(client, b"clear\necho CLEAR-DONE\n")
                clear_sent = True
                buffer.clear()

            if (clear_sent and not clear_ready and
                    b"CLEAR-DONE" in buffer and
                    time.time() - last_data > 0.2):
                if gate_dir is not None:
                    open(os.path.join(gate_dir, "clear-ready"), "w").close()
                clear_ready = True

            if clear_ready and not clear_released:
                if gate_dir is None or os.path.exists(
                        os.path.join(gate_dir, "clear-continue")):
                    clear_released = True

            if clear_released and not ls_sent:
                send_line(client, b"ls /\n")
                ls_sent = True

            if ls_sent and not ls_error_sent and b"system/" in buffer:
                send_line(client, b"ls /missing\n")
                ls_error_sent = True

            if ls_error_sent and not spawn_error_sent and b"ls: list failed" in buffer:
                send_line(client, b".\n")
                spawn_error_sent = True

            if spawn_error_sent and not hello_sent and b"run: spawn failed" in buffer:
                send_line(client, b"hello\n")
                hello_sent = True

            if hello_sent and not halt_sent and b"hello from applications" in buffer:
                send_line(client, b"halt\n")
                halt_sent = True

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
