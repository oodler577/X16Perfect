"""Build the portable Windows package from a compiled PRG and the upstream emulator."""

import argparse
import hashlib
from pathlib import Path, PurePosixPath
from zipfile import ZIP_DEFLATED, ZipFile


ROOT = Path(__file__).resolve().parent.parent
RUNTIME_DIR = "x16emu-r49"
RUNTIME_SHA256 = "051047e59d86149e23471dce93f9f5511ef1e70897ac3a61b5b5a9c2cd397a1e"
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


def package(emulator, destination):
    with emulator.open("rb") as stream:
        if hashlib.file_digest(stream, "sha256").hexdigest() != RUNTIME_SHA256:
            raise ValueError("The input must be the official X16Emu R49 Win64 ZIP")

    launcher = (ROOT / "RUN-WINDOWS.bat").read_text(encoding="ascii")
    if "-prg \"X16PERF.PRG\" -run" not in launcher:
        raise ValueError("Windows launcher does not target X16PERF.PRG")

    with ZipFile(emulator) as upstream:
        runtime = {
            name
            for name in upstream.namelist()
            if PurePosixPath(name).name == name
            and (name in ("x16emu.exe", "rom.bin") or name.lower().endswith(".dll"))
        }
        if not {"x16emu.exe", "rom.bin", "SDL2.dll", "zlib1.dll", "libwinpthread-1.dll"} <= runtime:
            raise ValueError("Emulator archive is missing required runtime files")

        destination.parent.mkdir(parents=True, exist_ok=True)
        pending = destination.with_suffix(".tmp")
        try:
            with ZipFile(pending, "w", ZIP_DEFLATED) as output:
                output.writestr("RUN-WINDOWS.bat", launcher.replace("\n", "\r\n").encode("ascii"))
                for path in APPLICATION_FILES:
                    output.write(ROOT / path, f"x16perfect/{path}")
                for name in sorted(runtime):
                    output.writestr(f"{RUNTIME_DIR}/{name}", upstream.read(name))
                for notice in sorted((ROOT / "third-party" / RUNTIME_DIR).glob("LICENSE*")):
                    output.write(notice, f"{RUNTIME_DIR}/{notice.name}")

            with ZipFile(pending) as output:
                if output.testzip() is not None:
                    raise ValueError("Packaged ZIP failed its integrity check")
            pending.replace(destination)
        finally:
            pending.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("emulator", type=Path, help="Official x16emu_win64-r49.zip")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    package(args.emulator, args.output)


if __name__ == "__main__":
    main()
