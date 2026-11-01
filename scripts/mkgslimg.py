#!/usr/bin/env python3
"""Build or verify a GSLFS read-only image.

GSLFS stores a superblock, a fixed node table, a name table, and contiguous
file data. All integers are little-endian. Checksums are standard CRC32
(reflected polynomial 0xEDB88320).

Usage:
  mkgslimg.py ROOT IMAGE
  mkgslimg.py --verify IMAGE
"""

import os
import stat
import struct
import sys
import zlib

MAGIC = b"GSLFS1\0\0"
VERSION = 1
BLOCK_SIZE = 4096
NODE_SIZE = 56
MAX_NODES = 128
MAX_NAME = 63

TYPE_FILE = 0
TYPE_DIR = 1

SUPERBLOCK_WITHOUT_CRC = 56
NODE_WITHOUT_CRC = 48


def align_up(value, align):
    return (value + align - 1) & ~(align - 1)


def crc32(data):
    return zlib.crc32(data) & 0xFFFFFFFF


def fail(message):
    print("mkgslimg: " + message, file=sys.stderr)
    raise SystemExit(1)


def collect_paths(root):
    if not os.path.isdir(root):
        fail("root is not a directory: " + root)
    paths = [""]
    for current, directories, files in os.walk(root):
        directories.sort()
        files.sort()
        for name in directories + files:
            full = os.path.join(current, name)
            if os.path.islink(full):
                fail("symlink is not supported: " + full)
            relative = os.path.relpath(full, root).replace(os.sep, "/")
            paths.append(relative)
    paths = sorted(set(paths))
    if paths[0] != "":
        fail("root must be the first node")
    if len(paths) > MAX_NODES:
        fail("too many nodes: %d (max %d)" % (len(paths), MAX_NODES))
    return paths


def parent_of(relative):
    if relative == "":
        return ""
    index = relative.rfind("/")
    if index < 0:
        return ""
    return relative[:index]


def build(root):
    paths = collect_paths(root)
    index_of = {path: index for index, path in enumerate(paths)}

    name_table = bytearray()
    name_offsets = {}
    for relative in paths:
        name = "" if relative == "" else relative.split("/")[-1]
        encoded = name.encode("utf-8")
        if len(encoded) > MAX_NAME:
            fail("name too long: " + relative)
        if "/" in name:
            fail("name contains slash: " + name)
        name_offsets[relative] = len(name_table)
        name_table += encoded + b"\0"

    node_offset = BLOCK_SIZE
    node_table_size = align_up(len(paths) * NODE_SIZE, BLOCK_SIZE)
    name_offset = node_offset + node_table_size
    name_table_size = align_up(len(name_table), BLOCK_SIZE)
    data_offset = name_offset + name_table_size
    first_data_block = data_offset // BLOCK_SIZE

    nodes = []
    data_chunks = []
    next_block = first_data_block

    for relative in paths:
        full = root if relative == "" else os.path.join(root, *relative.split("/"))
        is_directory = relative == "" or os.path.isdir(full)
        if not is_directory and not os.path.isfile(full):
            fail("unsupported file type: " + full)

        mode = stat.S_IMODE(os.stat(full).st_mode)
        if is_directory:
            node_type = TYPE_DIR
            size = 0
            data_block = 0
            block_count = 0
            data_checksum = 0
        else:
            node_type = TYPE_FILE
            with open(full, "rb") as handle:
                data = handle.read()
            size = len(data)
            block_count = (size + BLOCK_SIZE - 1) // BLOCK_SIZE
            data_block = next_block
            next_block += block_count
            data_checksum = crc32(data) if size else 0
            padded = data + bytes((block_count * BLOCK_SIZE) - size)
            data_chunks.append((data_block, padded))

        parent = index_of[parent_of(relative)]
        header = struct.pack(
            "<6I3Q",
            node_type,
            mode,
            0,
            0,
            parent,
            name_offsets[relative],
            size,
            data_block,
            block_count,
        )
        checksum = crc32(header)
        nodes.append(header + struct.pack("<2I", checksum, data_checksum))

    block_count_total = next_block
    if block_count_total < 2:
        block_count_total = 2

    superblock_header = struct.pack(
        "<8sIIQIIQQQ",
        MAGIC,
        VERSION,
        BLOCK_SIZE,
        block_count_total,
        len(paths),
        0,
        node_offset,
        name_offset,
        data_offset,
    )
    superblock = superblock_header + struct.pack("<I", crc32(superblock_header))

    image = bytearray(block_count_total * BLOCK_SIZE)
    image[0:len(superblock)] = superblock
    node_table = b"".join(nodes)
    image[node_offset:node_offset + len(node_table)] = node_table
    image[name_offset:name_offset + len(name_table)] = name_table
    for data_block, data in data_chunks:
        start = data_block * BLOCK_SIZE
        image[start:start + len(data)] = data
    return bytes(image), len(paths), block_count_total


def read_u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]


def read_u64(data, offset):
    return struct.unpack_from("<Q", data, offset)[0]


def verify(path):
    with open(path, "rb") as handle:
        image = handle.read()
    if len(image) < BLOCK_SIZE:
        fail("image is too small")
    if image[0:8] != MAGIC:
        fail("bad magic")
    version = read_u32(image, 8)
    block_size = read_u32(image, 12)
    block_count = read_u64(image, 16)
    node_count = read_u32(image, 24)
    root_node = read_u32(image, 28)
    node_offset = read_u64(image, 32)
    name_offset = read_u64(image, 40)
    data_offset = read_u64(image, 48)
    if version != VERSION or block_size != BLOCK_SIZE:
        fail("unsupported version or block size")
    if block_count == 0 or block_count * BLOCK_SIZE > len(image):
        fail("bad block count")
    if crc32(image[0:SUPERBLOCK_WITHOUT_CRC]) != read_u32(image, 56):
        fail("superblock checksum mismatch")
    if node_count == 0 or node_count > MAX_NODES or root_node >= node_count:
        fail("bad node count or root")
    if node_offset + node_count * NODE_SIZE > len(image):
        fail("node table outside image")
    if name_offset >= data_offset or data_offset > len(image):
        fail("bad table offsets")
    if data_offset % BLOCK_SIZE != 0:
        fail("data offset is not block aligned")

    for index in range(node_count):
        offset = node_offset + index * NODE_SIZE
        node = image[offset:offset + NODE_SIZE]
        if len(node) != NODE_SIZE:
            fail("truncated node table")
        if crc32(node[:NODE_WITHOUT_CRC]) != read_u32(node, 48):
            fail("node checksum mismatch at %d" % index)
        node_type = read_u32(node, 0)
        parent = read_u32(node, 16)
        name_off = read_u32(node, 20)
        size = read_u64(node, 24)
        data_block = read_u64(node, 32)
        blocks = read_u64(node, 40)
        data_checksum = read_u32(node, 52)
        if node_type not in (TYPE_FILE, TYPE_DIR):
            fail("bad node type at %d" % index)
        if parent >= node_count:
            fail("bad parent at %d" % index)
        if name_offset + name_off >= data_offset:
            fail("name outside name table at %d" % index)
        end = image.find(b"\0", name_offset + name_off, data_offset)
        if end < 0:
            fail("unterminated name at %d" % index)
        name = image[name_offset + name_off:end]
        if len(name) > MAX_NAME or b"/" in name:
            fail("bad name at %d" % index)
        if node_type == TYPE_FILE:
            if blocks != ((size + BLOCK_SIZE - 1) // BLOCK_SIZE):
                fail("bad block count at %d" % index)
            if data_block < data_offset // BLOCK_SIZE:
                fail("data before data area at %d" % index)
            if data_block + blocks > block_count:
                fail("data outside image at %d" % index)
            start = data_block * BLOCK_SIZE
            data = image[start:start + size]
            if size != 0 and crc32(data) != data_checksum:
                fail("data checksum mismatch at %d" % index)

    print(
        "mkgslimg: verified nodes=%d bytes=%d" % (node_count, len(image))
    )


def main(argv):
    if len(argv) == 3 and argv[1] == "--verify":
        verify(argv[2])
        return 0
    if len(argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    image, node_count, block_count = build(argv[1])
    with open(argv[2], "wb") as handle:
        handle.write(image)
    print(
        "mkgslimg: nodes=%d blocks=%d bytes=%d"
        % (node_count, block_count, len(image))
    )
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
