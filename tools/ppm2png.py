#!/usr/bin/env python3
"""Minimal PPM to PNG, so previews can be looked at without ImageMagick."""
import struct, sys, zlib


def main(src, dst, scale=1):
    with open(src, "rb") as fh:
        assert fh.readline().strip() == b"P6"
        line = fh.readline()
        while line.startswith(b"#"):
            line = fh.readline()
        width, height = (int(v) for v in line.split())
        assert int(fh.readline()) == 255
        data = fh.read()

    rows = []
    for y in range(height):
        row = data[y * width * 3:(y + 1) * width * 3]
        if scale > 1:
            row = b"".join(row[x * 3:x * 3 + 3] * scale for x in range(width))
        rows.extend([b"\x00" + row] * scale)
    raw = b"".join(rows)

    def chunk(tag, payload):
        return (struct.pack(">I", len(payload)) + tag + payload +
                struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", width * scale, height * scale, 8, 2, 0, 0, 0)) +
           chunk(b"IDAT", zlib.compress(raw, 9)) +
           chunk(b"IEND", b""))
    with open(dst, "wb") as fh:
        fh.write(png)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 1)
