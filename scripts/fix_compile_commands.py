#!/usr/bin/env python3
"""Rewrite container paths (/work) in compile_commands.json to host paths.

CMake runs inside podman with the repo mounted at /work, so every entry
points at /work/... On the host those paths do not exist and clangd or
cpptools cannot resolve includes. This rewrites them to the repo root.

A /work component is only treated as the container root when it starts a
path token or follows a flag such as -I. Paths like /home/runner/work must
stay untouched.

Usage: fix_compile_commands.py <compile_commands.json> [repo_root]
Exit nonzero on invalid JSON or missing file.
"""

import json
import os
import sys

CONTAINER_ROOT = "/work"


def fail(msg):
    print("fix_compile_commands: " + msg)
    return 1


def rewrite_token(token, repo_root):
    """Replace each /work path root in one space-separated token."""
    changed = False
    position = 0
    while True:
        index = token.find(CONTAINER_ROOT, position)
        if index < 0:
            break
        prefix = token[:index]
        if "/" in prefix:
            position = index + len(CONTAINER_ROOT)
            continue
        token = (
            prefix
            + repo_root
            + token[index + len(CONTAINER_ROOT):]
        )
        position = index + len(repo_root)
        changed = True
    return token, changed


def rewrite_value(value, repo_root):
    """Rewrite every path token, preserving the command separators."""
    parts = value.split(" ")
    changed = 0
    for index, part in enumerate(parts):
        rewritten, part_changed = rewrite_token(part, repo_root)
        if part_changed:
            parts[index] = rewritten
            changed += 1
    return " ".join(parts), changed


def main(argv):
    if len(argv) < 2 or len(argv) > 3:
        return fail("usage: fix_compile_commands.py <db.json> [repo_root]")
    db_path = argv[1]
    repo_root = argv[2] if len(argv) == 3 else os.getcwd()

    if not os.path.isfile(db_path):
        return fail("missing file: " + db_path)

    try:
        with open(db_path, "r", encoding="ascii") as f:
            entries = json.load(f)
    except (OSError, ValueError) as e:
        return fail("cannot read json: " + str(e))

    if not isinstance(entries, list):
        return fail("json root is not a list")

    changed = 0
    for entry in entries:
        for key in ("directory", "command", "file", "output"):
            value = entry.get(key)
            if not isinstance(value, str):
                continue
            rewritten, count = rewrite_value(value, repo_root)
            if count == 0:
                continue
            # Container compiler paths do not exist on the host. Use the
            # bare name so clangd resolves it from PATH.
            if rewritten.startswith("/usr/bin/clang++ "):
                rewritten = "clang++ " + rewritten[len("/usr/bin/clang++ "):]
            elif rewritten.startswith("/usr/bin/clang "):
                rewritten = "clang " + rewritten[len("/usr/bin/clang "):]
            if rewritten != value:
                entry[key] = rewritten
                changed += 1

    try:
        with open(db_path, "w", encoding="ascii") as f:
            json.dump(entries, f, indent=2)
            f.write("\n")
    except OSError as e:
        return fail("cannot write json: " + str(e))

    print("fix_compile_commands: rewrote %d fields to %s" % (changed, repo_root))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
