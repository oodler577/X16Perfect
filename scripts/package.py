"""Build the portable Windows package from a compiled X16Perfect PRG."""

import argparse
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


ROOT = Path(__file__).resolve().parent.parent
APPLICATION_FILES = (
    "X16PERF.PRG",
    "Makefile",
    "README.md",
    "COVERAGE.md",
    "document.c",
    "document.h",
    "editor.c",
    "machine.s",
    "tests/storage.c",
    "tests/make-fixture.py",
    "tests/check-storage.py",
)


def package(destination):
    launcher = (ROOT / "RUN-WINDOWS.bat").read_text(encoding="ascii")
    if '-prg "X16PERF.PRG" -run' not in launcher:
        raise ValueError("Windows launcher does not target X16PERF.PRG")

    destination.parent.mkdir(parents=True, exist_ok=True)
    pending = destination.with_suffix(".tmp")
    try:
        with ZipFile(pending, "w", ZIP_DEFLATED) as output:
            output.writestr("RUN-WINDOWS.bat", launcher.replace("\n", "\r\n").encode("ascii"))
            for path in APPLICATION_FILES:
                output.write(ROOT / path, f"x16perfect/{path}")

        with ZipFile(pending) as output:
            if output.testzip() is not None:
                raise ValueError("Packaged ZIP failed its integrity check")
        pending.replace(destination)
    finally:
        pending.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    package(args.output)


if __name__ == "__main__":
    main()
