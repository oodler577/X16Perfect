# X16Perfect

X16Perfect is a native 65C02 WordPerfect-compatible editor milestone for the Commander X16. The build uses an original interface and the X16 ROM font.

## Windows quick start

Extract the X16Perfect ZIP to a writable folder. Download the [Windows build of X16Emu R49](https://github.com/X16Community/x16-emulator/releases/tag/r49) separately and extract it into an `x16emu-r49` folder next to `RUN-WINDOWS.bat`. Place `x16emu.exe`, `rom.bin`, and the DLLs directly in that folder, then double-click the launcher. It uses 8 MHz, 512 KiB of RAM, and 2× display scaling. Files opened or saved through device 8 are in the `x16perfect` folder next to `X16PERF.PRG`. Use F5 to list files and F10 to save under a new filename. If you are upgrading from Folio/16, copy your saved documents to the new `x16perfect` folder.


## Build and run

With cc65 installed, run `make`. The tested compiler is Debian cc65 2.19. Run the same PRG in X16Emu R49 with its matching ROM. For example, from this directory:

```sh
x16emu -c02 -mhz 2 -ram 512 -sdcard working.img -prg X16PERF.PRG -run
x16emu -c02 -mhz 8 -ram 512 -sdcard working.img -prg X16PERF.PRG -run
x16emu -c02 -mhz 8 -ram 2048 -sdcard working.img -prg X16PERF.PRG -run
```

Use a disposable copy of a FAT32 image. The X16Emu distribution provides an empty SD image. To boot on an X16, load the PRG from device 8 and run it from BASIC:

```text
LOAD "X16PERF.PRG",8
RUN
```

## Editor

- F3 opens Commands. Choose R to retrieve, L for layout commands, P for Print, S for Printer Setup, or N for New. F5 opens a list of up to 24 accessible files. Enter selects a file. F1 cancels modal prompts.
- F6 toggles bold and F8 toggles underline. X16Perfect represents these with text colors. Alt+F3 or F11 toggles the inline code view. Use Left/Right to move across code records and Backspace to remove a record to the left.
- F2 performs literal forward search. Shift+F2 can replace a single match. Alt+F4 or F12 marks a block, with a 2 KiB clipboard available through Ctrl+C, Ctrl+X and Ctrl+V.
- F10 saves to a fresh filename. To save edits to a retrieved document, supply a distinct filename. Existing files are protected. A failed write may leave a partial new file. F7 offers to save unsaved edits, then asks for exit confirmation.
- Layout entries insert document margin records and hard page breaks. Margin fields use integer 1200ths of an inch. The editor’s screen wrap has its own dimensions.
- Shift+F7 opens Print. Print and Printer Setup are navigable, intentionally inactive menus. They perform no output or device configuration.

A banked gap buffer holds the document. A separate banked staging region protects the current buffer while another file is read and checked. The reader supports a subset of unencrypted WP 5.x documents in the single-byte character workflow. It preserves the imported prefix verbatim for editable files. Files with unsupported inline records can open read-only, and malformed records cause retrieval to fail. The preview approximates layout and may omit unsupported formatting and character glyphs.

## Document size

The document body can hold 240 KiB with 512 KiB of banked RAM or 1,008 KiB with 2 MiB of banked RAM. Formatting codes count toward this body allowance. The imported WP5 prefix has a separate 8 KiB limit. The storage tests verified formatted bodies of 128 KiB with 512 KiB of RAM and 512 KiB with 2 MiB of RAM. The limits are derived from the memory layout.

## Verification

See [COVERAGE.md](COVERAGE.md). To build the native storage diagnostic, run `make STORAGE.PRG`. It expects `BIG.WPD` on device 8. Generate a fixture with `python3 tests/make-fixture.py 128 BIG.WPD` (or use 64 or 512). The fixture must contain supported records, have more than 8192 body bytes, contain the literal `unique phrase`, and have a character token at body offset 8191. Use a disposable image without an existing `LARGEOUT.WPD`. With X16Emu add `-echo iso` to see the reported KERNAL ticks. Use `python3 tests/check-storage.py BIG.WPD LARGEOUT.WPD` to check both insertions byte for byte.

## References

- [WordPerfect 5.1 Developer’s Toolkit](https://mendelson.org/wpdos/WP51DevelopersToolkit.pdf)
- [WordPerfect 5.1 reference manual](https://mendelson.org/wpdos/WPDOS51Reference.pdf)
- [Commander X16 reference manual](https://github.com/X16Community/x16-docs)
- [X16Emu R49](https://github.com/X16Community/x16-emulator/releases/tag/r49)

## CI packages

GitHub Actions builds the native PRG and storage diagnostic on pushes and pull requests targeting `master`. The `X16Perfect` artifact contains the standalone PRG and the Windows ZIP. The ZIP contains the application and launcher.

To produce the same Windows package locally, build with `make` and run `python3 scripts/package.py dist/X16Perfect-Windows.zip`.

Pushing a `v*` tag also publishes the same PRG and Windows ZIP on [GitHub Releases](https://github.com/oodler577/X16Perflect/releases). Tags with a suffix such as `-preview.1` publish as prereleases.
