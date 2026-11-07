#!/usr/bin/env python3
"""Rewrite container paths (/work) in compile_commands.json to host paths.

CMake runs inside podman with the repo mounted at /work, so every entry
points at /work/... On the host those paths do not exist and clangd or
cpptools cannot resolve includes. This rewrites them to the repo root.

Usage: fix_compile_commands.py <compile_commands.json> [repo_root]
Exit nonzero on invalid JSON or missing file.
"""

import json
import os
import re
import sys

# Only rewrite /work when it is a complete path component and not part of
# another path such as /home/runner/work.
WORK_PREFIX = re.compile(r"(?<![A-Za-z0-9_.-])/work(?![A-Za-z0-9_.-])")


def fail(msg):
    print("fix_compile_commands: " + msg)
    return 1


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
            original = value
            if "/work" in value:
                value = WORK_PREFIX.sub(lambda _match: repo_root, value)
            # Container compiler paths do not exist on the host. Use the
            # bare name so clangd resolves it from PATH.
            if value.startswith("/usr/bin/clang++ "):
                value = "clang++ " + value[len("/usr/bin/clang++ "):]
            elif value.startswith("/usr/bin/clang "):
                value = "clang " + value[len("/usr/bin/clang "):]
            if value != original:
                entry[key] = value
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
