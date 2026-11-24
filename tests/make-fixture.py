"""Generate an original WP5 diagnostic document."""
from argparse import ArgumentParser
from pathlib import Path

parser = ArgumentParser(description=__doc__)
parser.add_argument("body_kib", type=int, choices=(64, 128, 512))
parser.add_argument("path", type=Path)
args = parser.parse_args()
prefix = bytes.fromhex("""
    FF 57 50 43 4C 00 00 00 01 0A 00 00 00 00 00 00
    FB FF 05 00 32 00 00 00 00 00 06 00 08 00 00 00
    42 00 00 00 08 00 02 00 00 00 4A 00 00 00 00 00
    00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
    00 00 08 00 7C 00 78 00 00 00 00 00
""")
row = b"plain text with \xc3\x0c\xc3bold\xc4\x0c\xc4 text.\x0a"
marker = b"unique phrase\x0a"
padding = next(n for n in range(len(row))
               if all(32 <= row[(i - n) % len(row)] <= 126 for i in (8191, 8192, 8193)))
whole, remainder = divmod(args.body_kib * 1024 - len(marker) - padding, len(row))
args.path.write_bytes(prefix + b"x" * padding + row * whole + b"x" * remainder + marker)
