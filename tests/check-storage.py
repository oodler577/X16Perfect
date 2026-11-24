"""Compare the native storage diagnostic's input and output."""
from argparse import ArgumentParser
from pathlib import Path

parser = ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
source = args.source.read_bytes()
offset = int.from_bytes(source[4:8], "little")
header, body = source[:offset], source[offset:]
expected = header + b"X16" + body[:8191] + b"X16" + body[8191:]
if args.output.read_bytes() != expected:
    raise SystemExit("Output differs from the expected two insertions")
print(f"{len(body)} source body bytes, verified byte for byte")
