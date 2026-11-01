#!/usr/bin/env python3
"""Compile-check every entry in build/compile_commands.json without linking.

This is a host-side safety net for environments without podman, the AArch64
linker, or QEMU. It uses the exact flags and include paths from the last
successful CMake configure, but asks clang for syntax-only compilation.
"""

import json
import os
import shlex
import subprocess
import sys


def transform(command, file_path):
    parts = shlex.split(command)
    result = []
    index = 0
    while index < len(parts):
        if parts[index] == "-o":
            index += 2
            continue
        if parts[index] == "-c":
            index += 1
            continue
        result.append(parts[index])
        index += 1
    result.append("-fsyntax-only")
    return result


def main(argv):
    if len(argv) > 2:
        print("usage: compile-check.py [file-substring]", file=sys.stderr)
        return 2
    filter_text = argv[1] if len(argv) == 2 else None

    try:
        with open("build/compile_commands.json", "r", encoding="utf-8") as handle:
            entries = json.load(handle)
    except OSError as error:
        print("compile-check: cannot read compile_commands.json: " + str(error),
              file=sys.stderr)
        return 2

    checked = 0
    failed = 0
    skipped = 0
    for entry in entries:
        file_path = entry["file"]
        if filter_text and filter_text not in file_path:
            continue
        if not os.path.isfile(file_path):
            skipped += 1
            continue
        command = transform(entry["command"], file_path)
        result = subprocess.run(
            command, cwd=entry["directory"], capture_output=True, text=True)
        checked += 1
        if result.returncode != 0:
            failed += 1
            print("compile-check: FAIL " + file_path)
            print(result.stderr.rstrip())
        elif filter_text:
            print("compile-check: OK " + file_path)

    if failed:
        print("compile-check: %d/%d failed (%d skipped)" % (failed, checked, skipped))
        return 1
    print("compile-check: %d files OK (%d skipped)" % (checked, skipped))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
