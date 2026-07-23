#!/usr/bin/env python3
"""Recursively extract an XDVDFS disc image owned by the user.

Usage: python3 xdvdfs_extract_all.py <disc-image> <destination> [--list]

The extractor walks the XDVDFS directory tree, preserves subdirectories, skips
already complete files, and resumes partially copied files. Extracted content is
local copyrighted game data and must never be committed or distributed.
"""

import os
import struct
import sys


SECTOR_SIZE = 2048
# Common game-partition bases: redump XGD2, XGD3 variants, and raw images.
CANDIDATE_BASES = [0xFD90000, 0x2080000, 0x18300000, 0]


def find_base(source):
    for base in CANDIDATE_BASES:
        try:
            source.seek(base + 0x10000)
            if source.read(20) == b"MICROSOFT*XBOX*MEDIA":
                return base
        except OSError:
            continue
    raise SystemExit("XDVDFS partition not found (missing MICROSOFT*XBOX*MEDIA magic)")


def read_dir(source, base, sector, size):
    """Return directory entries as (name, sector, size, attributes)."""
    source.seek(base + sector * SECTOR_SIZE)
    data = source.read(size)
    entries = []
    seen = set()

    def walk(offset):
        if offset + 14 > len(data) or offset in seen:
            return
        seen.add(offset)
        left, right, start, file_size, attributes, name_length = struct.unpack_from(
            "<HHIIBB", data, offset
        )
        if left == 0xFFFF and right == 0xFFFF:
            return
        name = data[offset + 14 : offset + 14 + name_length].decode(
            "ascii", "replace"
        )
        if name:
            entries.append((name, start, file_size, attributes))
        if left:
            walk(left * 4)
        if right:
            walk(right * 4)

    walk(0)
    return entries


def copy_file(source, base, sector, size, output_path):
    """Resume an incomplete copy instead of restarting a large file."""
    copied = os.path.getsize(output_path) if os.path.exists(output_path) else 0
    if copied > size:
        copied = 0
    source.seek(base + sector * SECTOR_SIZE + copied)
    remaining = size - copied
    with open(output_path, "r+b" if copied else "wb") as output:
        output.seek(copied)
        while remaining > 0:
            chunk = source.read(min(8 << 20, remaining))
            if not chunk:
                raise IOError(f"truncated source while copying {output_path}")
            output.write(chunk)
            remaining -= len(chunk)


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)

    image_path, destination = sys.argv[1], sys.argv[2]
    list_only = "--list" in sys.argv

    with open(image_path, "rb") as source:
        base = find_base(source)
        source.seek(base + 0x10000 + 20)
        root_sector, root_size = struct.unpack("<II", source.read(8))
        print(
            f"partition_base=0x{base:X} root_sector={root_sector} "
            f"root_size={root_size}"
        )

        total_files = 0
        total_bytes = 0

        def recurse(sector, size, relative):
            nonlocal total_files, total_bytes
            for name, start, file_size, attributes in sorted(
                read_dir(source, base, sector, size)
            ):
                relative_path = os.path.join(relative, name) if relative else name
                if attributes & 0x10:
                    if not list_only:
                        os.makedirs(
                            os.path.join(destination, relative_path), exist_ok=True
                        )
                    print(f"DIR   {relative_path}/")
                    if file_size:
                        recurse(start, file_size, relative_path)
                    continue

                output_path = os.path.join(destination, relative_path)
                total_files += 1
                total_bytes += file_size
                if list_only:
                    print(f"FILE  {file_size:>13,}  {relative_path}")
                    continue
                if os.path.exists(output_path) and os.path.getsize(output_path) == file_size:
                    continue
                os.makedirs(os.path.dirname(output_path) or destination, exist_ok=True)
                copy_file(source, base, start, file_size, output_path)
                print(f"OK    {file_size:>13,}  {relative_path}", flush=True)

        if not list_only:
            os.makedirs(destination, exist_ok=True)
        recurse(root_sector, root_size, "")
        print(f"\ntotal: {total_files} files, {total_bytes:,} bytes")


if __name__ == "__main__":
    main()
