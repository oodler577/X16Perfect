"""Build the portable Windows package from a compiled X16Perfect PRG."""

import argparse
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


ROOT = Path(__file__).resolve().parent.parent


def package(destination):
    bundle_files = {
        "X16PERF.PRG": ROOT / "X16PERF.PRG",
        "RUN-WINDOWS.bat": ROOT / "RUN-WINDOWS.bat",
        "README.txt": ROOT / "packaging" / "README.txt",
        "FORMAT.WPD": ROOT / "packaging" / "demos" / "FORMAT.WPD",
        "WELCOME.TXT": ROOT / "packaging" / "demos" / "WELCOME.TXT",
    }

    destination.parent.mkdir(parents=True, exist_ok=True)
    pending = destination.with_suffix(".tmp")
    try:
        with ZipFile(pending, "w", ZIP_DEFLATED) as output:
            for name, path in bundle_files.items():
                if path.suffix.lower() in {".bat", ".txt"}:
                    text = path.read_text(encoding="ascii").replace("\n", "\r\n")
                    output.writestr(name, text.encode("ascii"))
                else:
                    output.write(path, name)

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
