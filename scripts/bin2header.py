#!/usr/bin/env python3
"""Convert a flat binary into a constexpr C++ header.

Usage: bin2header.py <input.bin> <output.h> [namespace]
Exit nonzero if the input is missing or unreadable.
"""

import sys


def fail(message):
    print("bin2header: " + message)
    return 1


def main(argv):
    if len(argv) < 3 or len(argv) > 4:
        return fail("usage: bin2header.py <input.bin> <output.h> [namespace]")
    source = argv[1]
    target = argv[2]
    namespace = argv[3] if len(argv) == 4 else "og::user"

    try:
        with open(source, "rb") as handle:
            data = handle.read()
    except OSError as error:
        return fail("cannot read input: " + str(error))

    if not data:
        return fail("input is empty")

    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("// Generated file. Do not edit.")
    lines.append("")
    lines.append("#include <cstdint>")
    lines.append("")
    lines.append("namespace " + namespace + " {")
    lines.append("")
    lines.append("inline constexpr uint8_t kProgram[] = {")
    for offset in range(0, len(data), 12):
        chunk = data[offset:offset + 12]
        body = ", ".join("0x%02x" % value for value in chunk)
        lines.append("  " + body + ",")
    lines.append("};")
    lines.append("")
    lines.append("inline constexpr uint64_t kProgramSize = %d;" % len(data))
    lines.append("")
    lines.append("}  // namespace " + namespace)
    lines.append("")

    try:
        with open(target, "w", encoding="ascii") as handle:
            handle.write("\n".join(lines))
    except OSError as error:
        return fail("cannot write output: " + str(error))

    print("bin2header: %d bytes -> %s" % (len(data), target))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
