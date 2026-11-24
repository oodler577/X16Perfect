# X16Perfect milestone coverage

X16Perfect is an original X16 implementation of an editable WP 5.x subset. It does not yet meet the requested full DOS WordPerfect muscle-memory acceptance test.

| Workflow | Native coverage | Remaining work |
|---|---|---|
| F5 List Files | Device 8 pattern prompt, sorted results, option 1 retrieves, No stays in the list | Look, the other file-list actions, paging, directory navigation, and matching DOS selection and display behavior |
| F6 Bold and F8 Underline | Inline WP on/off records with distinct screen colors | Block-specific formatting, nested styles, typographic rendering and DOS edge cases |
| Alt+F3 / F11 Reveal Codes | Inline code view with cursor movement and record deletion | Synchronized split view, full code names, original viewport and selection behavior |
| F10 Save / Shift+F10 Retrieve | WP 5.x subset and basic ASCII, opens into a blank document, inserts at the cursor in a nonempty document, confirmed replacement with No as the default | Complete format and conversion support, full prompt field editing, ASCII export, and reference filename/path handling |
| F7 Exit | Save decision, exit confirmation and cancel | Remaining original edge cases and exact default behavior |
| F2 Search / Shift+F2 Replace | Literal forward search and single replacement | Backward, repeat, replace all, match options, original flow |
| Alt+F4 Block | Mark selection and a bounded clipboard | Original move/copy menus, rectangular blocks and cross-document operations |
| Page and paragraph formatting | Document margin codes, hard breaks, soft left tabs, word wrap | Full layout engine, editing form size/orientation and font metadata, indents, tab stops and page numbering |
| Shift+F7 Print and printer setup | Navigable output and setup choices, intentionally inactive | Complete DOS submenu, prompt and cancel parity. Printing stays outside scope. |
| Advanced editing | Some tokens visible generically or protected by read-only access | Tables, styles, columns, headers, footnotes, graphics, macros, merge, undo and proofreading |
| WordPerfect 5.1 Plus additions | Original reference used | WP 6.x conversion, alternate conversion formats, additional file-list options, mail and fax paths |

Unsupported editable document cases include extended character rendering, printer-dependent formatting, complex packet references and protected/encrypted documents. See [REFERENCE.md](REFERENCE.md) for the first-level DOS inventory and file-format release gate. Nested menus, key variants, and command evidence remain incomplete.

## Verification

R49 was run with 65C02, its matching ROM, and FAT32 SD images. The formatted smoke document was retrieved, edited and saved as a separate file on each profile. Test tools checked that the exported bytes matched across profiles. libwpd recognized the text, bold and underline as separate formatted spans.

The native storage diagnostic uses the same document module as the editor. It opens a synthetic formatted WP 5.x document from an emulated SD card, scans for a marker near the end, inserts text at the start and across an 8 KiB bank boundary, and writes a new document. A host check compares the **entire file** with the source plus the two insertions. The diagnostic scan measures a straight document scan, which is distinct from the interactive F2 workflow.

Early GUI responsiveness measurements used 40 synthetic single keypresses on a blank document and observed changes in the VM's X11 framebuffer. The 2 MHz run measured about 68 ms p95, above the requested 50 ms. It observed a frame change for all 40 injections. It did not test held-key repeat, interrupted I/O, long-paragraph reflow, page-move timing or end-to-end latency on hardware. Physical display, keyboard and SD behavior require hardware testing.

| X16Emu profile | Formatted body | Open | Stream scan | Save | Byte comparison |
|---|---:|---:|---:|---:|---|
| 2 MHz / 512 KiB | 64 KiB | 18.70 s | 14.20 s | 7.02 s | Passed |
| 2 MHz / 512 KiB | 128 KiB | 37.23 s | 28.38 s | 13.83 s | Passed |
| 8 MHz / 512 KiB | 128 KiB | 8.78 s | 6.57 s | 3.92 s | Passed |
| 8 MHz / 2 MiB | 512 KiB | 35.05 s | 26.23 s | 15.57 s | Passed |

Times are KERNAL ticks divided by 60. Save timing excludes the preceding gap relocation and insert operations. The three profiles used the same storage diagnostic binary.

### F10 replacement

WordPerfect 5.1 Plus offers the current filename, then asks whether to replace an existing file with No as the default. The X16Perfect F10 replay on R49 HostFS covered a new filename, the prefilled filename, Enter, N, Esc and Y at the replacement prompt. File comparisons checked that declining or canceling kept the original bytes and that Y wrote the edited document.

Build the native diagnostic with `make SAVETEST.PRG` and run it in an empty HostFS working directory or with an empty writable SD image. It creates `CHECK.WPD` and two files whose names match replacement scratch names. It verifies their contents after replacement and reports `SAVE REPLACE PASS`. The same binary passed on all three SD profiles and on HostFS at 8 MHz / 512 KiB. On a full SD image at 8 MHz / 512 KiB, replacement failed as expected and a host comparison confirmed that `FULL.WPD` remained byte-identical. Recovery after interrupted power and behavior on physical media remain unverified.

### Retrieve replay

DOSBox 0.74-3 ran the English 5.1 Plus media dated March 21, 1994. Shift+F10 displays a filename prompt when the block is off. An empty document opens at its start and takes the filename. Retrieval into a nonempty document inserts at the cursor, which stays before the insertion, and F10 keeps the active filename. An unnamed destination remains unnamed. F1 cancels the filename prompt. With a block active, Shift+F10 and F5 leave the selection active. A missing file leaves the document intact. The DOS reference imported the two-line ASCII fixture. X16Perfect rejects malformed WordPerfect records and unsupported binary data.

In DOS, F5 first offers a directory pattern. Option 1 retrieves the selected file, with no insert confirmation for an empty document. For a nonempty document, Y inserts at the current position. Enter or No keeps the listing open. DOS also offers Look as the default action and displays directory entries. X16Perfect verifies the retrieve action against the same rules and still needs the other list actions and directory traversal.

R49 HostFS editor replays verified the prompt, blank open, in-place formatted insertion, cursor and name retention, cancellation, missing and malformed inputs, block gating, F5 decline and confirm, ASCII import, and F10 default and confirmed replacement. The emulator replay used F12 to activate Block, since the emulator closes on Alt+F4. The native `RETRIEVE.PRG` diagnostic adds an insertion spanning an 8 KiB bank boundary, failed-insert preservation, and an ASCII CRLF crossing the input header boundary. Run it on a disposable empty device 8. Both the Retrieve and F10 diagnostics passed on HostFS and on SD in all three profiles, and the full-card test retained the original file byte for byte.

### DOS exchange

WordPerfect 5.1 Plus opened the native editor's formatted fixture in DOSBox 0.74-3 and saved an additional character. The X16 document module then loaded that DOS-saved file from emulated SD, inserted another character and saved it. A byte comparison verified its entire 335-byte DOS prefix and every original formatting code. The returned file was retrieved and displayed by WordPerfect. An independent libwpd check verified the returned text and both attributes.

The current replay also round-tripped a combined formatted and ASCII document. DOSBox displayed the X16-created file and added a character. X16Perfect opened the DOS save, inserted another character, and saved it. The entire 335-byte DOS prefix and all existing body bytes matched. DOSBox reopened the returned file. See the format inventory for cases outside these fixtures.

The preview.2 release PRG rebuilt byte for byte from a clean build. SHA-256 is `fbec2ca06a77d2d451e463fafb7b206f45772f6cd201b4427f37aa6b050b0a69`.
