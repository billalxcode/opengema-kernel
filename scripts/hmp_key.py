#!/usr/bin/env python3
"""Type a string through the HMP monitor after a delay.

Usage: hmp_key.py <monitor-socket> <delay-seconds> <text>
"""

import socket
import sys
import time


def connect(path, deadline):
    client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    while True:
        try:
            client.connect(path)
            return client
        except OSError:
            if time.time() >= deadline:
                return None
            time.sleep(0.2)


def key_name(value):
    if value == "\n":
        return "ret"
    if value == " ":
        return "spc"
    return value


def main(argv):
    if len(argv) != 4:
        print("usage: hmp_key.py <monitor-socket> <delay-seconds> <text>")
        return 2
    path = argv[1]
    delay = float(argv[2])
    text = argv[3]

    client = connect(path, time.time() + 5.0)
    if client is None:
        print("hmp key: connect failed")
        return 1
    client.settimeout(0.2)
    try:
        client.recv(4096)
    except OSError:
        pass

    time.sleep(delay)
    if not text.endswith("\n"):
        text += "\n"
    for value in text:
        client.sendall(("sendkey %s\n" % key_name(value)).encode())
        time.sleep(0.05)
        try:
            response = client.recv(4096)
            if b"unknown" in response.lower() or b"error" in response.lower():
                print("hmp key:", response.decode(errors="replace").strip())
        except OSError:
            pass
    print("hmp key: sent", len(text), "keys")
    client.close()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
