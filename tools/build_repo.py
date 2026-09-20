#!/usr/bin/env python3
"""
Builds repo.bin: a small on-disk "app store" for Flolower OS.

Layout written to repo.bin (which the Makefile then dd's into
flolower.img at REPO_START_LBA):

  Sector 0 (index):
    magic      "FLOPREPO"   8 bytes
    count      u32 LE       number of packages
    entries[count]:
      name       char[32]   NUL-padded
      lba        u32 LE     sector offset from REPO_START_LBA
      sectors    u32 LE     length in 512-byte sectors

  Each package (sector-aligned, starting at its own lba):
    magic          "FLOP"   4 bytes
    name           char[32] NUL-padded
    glyph          char[4]  NUL-padded (desktop icon letter)
    content_type   u8       0 = plain text
    reserved       u8[3]
    content_length u32 LE
    content        bytes    (content_length bytes, rest of the
                             package's sectors zero-padded)

Every repo/*.txt file becomes one package, named after the filename
(without .txt). The glyph is the first letter of the name, uppercased.
"""
import os
import struct
import sys

SECTOR = 512
REPO_DIR = os.path.join(os.path.dirname(__file__), "..", "repo")
OUT_PATH = os.path.join(os.path.dirname(__file__), "..", "repo.bin")

MAX_ENTRIES = 12

def pack_package(name: str, content: bytes) -> bytes:
    glyph = (name[0].upper() if name else "?").encode("ascii", "replace")[:1]
    header = b"FLOP"
    header += name.encode("ascii", "replace")[:32].ljust(32, b"\0")
    header += glyph.ljust(4, b"\0")
    header += struct.pack("<B", 0)
    header += b"\0\0\0"
    header += struct.pack("<I", len(content))
    blob = header + content
    pad = (-len(blob)) % SECTOR
    return blob + b"\0" * pad

def main():
    if not os.path.isdir(REPO_DIR):
        print(f"no repo/ directory at {REPO_DIR}, nothing to build")
        return

    names = sorted(f[:-4] for f in os.listdir(REPO_DIR) if f.endswith(".txt"))
    if len(names) > MAX_ENTRIES:
        print(f"warning: {len(names)} packages, only the first {MAX_ENTRIES} fit the index sector")
        names = names[:MAX_ENTRIES]

    packages = []
    for name in names:
        with open(os.path.join(REPO_DIR, f"{name}.txt"), "rb") as f:
            content = f.read()
        packages.append((name, pack_package(name, content)))

    index = b"FLOPREPO"
    index += struct.pack("<I", len(packages))

    body = b""
    lba_cursor = 1
    entries = b""
    for name, blob in packages:
        sectors = len(blob) // SECTOR
        entries += name.encode("ascii", "replace")[:32].ljust(32, b"\0")
        entries += struct.pack("<II", lba_cursor, sectors)
        body += blob
        lba_cursor += sectors

    index += entries
    index = index.ljust(SECTOR, b"\0")

    with open(OUT_PATH, "wb") as f:
        f.write(index)
        f.write(body)

    total_sectors = len(index) // SECTOR + len(body) // SECTOR
    print(f"repo.bin: {len(packages)} package(s), {total_sectors} sectors")
    for name, blob in packages:
        print(f"  - {name}: {len(blob) // SECTOR} sector(s)")

if __name__ == "__main__":
    main()
