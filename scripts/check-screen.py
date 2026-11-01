#!/usr/bin/env python3
"""Capture the QEMU console and verify the framebuffer console drew text.

Usage: check-screen.py <monitor-socket> <ppm-path>
Exit nonzero if the capture is missing, too small, or has no text pixels.
"""

import os
import socket
import sys
import time

BACKGROUND = (16, 20, 28)
FOREGROUND = (232, 234, 240)
MIN_FOREGROUND = 2000


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


def request_dump(socket_path, ppm_path):
    client = connect(socket_path, time.time() + 5.0)
    if client is None:
        print("screen check: monitor connect failed")
        return False
    client.settimeout(0.5)
    try:
        client.recv(4096)
    except OSError:
        pass

    if os.path.exists(ppm_path):
        os.unlink(ppm_path)
    client.sendall(("screendump %s\n" % ppm_path).encode())

    deadline = time.time() + 5.0
    last_size = -1
    while time.time() < deadline:
        try:
            size = os.path.getsize(ppm_path)
        except OSError:
            size = -1
        if size > 0 and size == last_size:
            client.close()
            return True
        last_size = size
        time.sleep(0.2)
    client.close()
    print("screen check: screendump file missing")
    return False


def load_ppm(path):
    data = open(path, "rb").read()
    fields = []
    offset = 0
    while len(fields) < 4 and offset < len(data):
        while offset < len(data) and data[offset : offset + 1].isspace():
            offset += 1
        if data[offset : offset + 1] == b"#":
            while offset < len(data) and data[offset : offset + 1] != b"\n":
                offset += 1
            continue
        start = offset
        while offset < len(data) and not data[offset : offset + 1].isspace():
            offset += 1
        fields.append(data[start:offset])
    if len(fields) < 4 or fields[0] != b"P6":
        raise ValueError("not a P6 ppm")
    width = int(fields[1])
    height = int(fields[2])
    offset += 1
    pixels = data[offset:]
    if len(pixels) < width * height * 3:
        raise ValueError("truncated ppm")
    return width, height, pixels


def main(argv):
    if len(argv) != 3:
        print("usage: check-screen.py <monitor-socket> <ppm-path>")
        return 2

    if not request_dump(argv[1], argv[2]):
        return 1

    try:
        width, height, pixels = load_ppm(argv[2])
    except (OSError, ValueError) as error:
        print("screen check: %s" % error)
        return 1

    if width < 640 or height < 480:
        print("screen check: unexpected size %dx%d" % (width, height))
        return 1

    foreground = 0
    background = 0
    for index in range(0, width * height * 3, 3):
        pixel = (pixels[index], pixels[index + 1], pixels[index + 2])
        if pixel == FOREGROUND:
            foreground += 1
        elif pixel == BACKGROUND:
            background += 1

    if foreground < MIN_FOREGROUND:
        print("screen check: only %d text pixels" % foreground)
        return 1
    if background == 0:
        print("screen check: background missing")
        return 1

    print("screen check: %dx%d, %d text pixels" % (width, height, foreground))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
