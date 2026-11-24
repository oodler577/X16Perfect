# X16Perfect milestone coverage

X16Perfect is an original X16 implementation of an editable WP 5.x subset. It does not yet meet the requested full DOS WordPerfect muscle-memory acceptance test.

| Workflow | Native coverage | Remaining work |
|---|---|---|
| F5 List Files | Device 8 listing and retrieve by selection | Paging, more than 24 entries, directory navigation, sorting and original prompt details |
| F6 Bold and F8 Underline | Inline WP on/off records with distinct screen colors | Block-specific formatting, nested styles, typographic rendering and DOS edge cases |
| Alt+F3 / F11 Reveal Codes | Inline code view with cursor movement and record deletion | Synchronized split view, full code names, original viewport and selection behavior |
| F10 Save / Shift+F10 Retrieve | WP 5.x subset, format validation, protected open and Save As | Safe named replacement, original prompts and advanced packet semantics |
| F7 Exit | Save decision, exit confirmation and cancel | Remaining original edge cases and exact default behavior |
| F2 Search / Shift+F2 Replace | Literal forward search and single replacement | Backward, repeat, replace all, match options, original flow |
| Alt+F4 Block | Mark selection and a bounded clipboard | Original move/copy menus, rectangular blocks and cross-document operations |
| Page and paragraph formatting | Document margin codes, hard breaks, soft left tabs, word wrap | Full layout engine, editing form size/orientation and font metadata, indents, tab stops and page numbering |
| Shift+F7 Print and printer setup | Navigable output and setup choices, intentionally inactive | Complete DOS submenu, prompt and cancel parity. Printing stays outside scope. |
| Advanced editing | Some tokens visible generically or protected by read-only access | Tables, styles, columns, headers, footnotes, graphics, macros, merge, undo and proofreading |
| WordPerfect 5.1 Plus additions | Original reference used | WP 6.x conversion, alternate conversion formats, additional file-list options, mail and fax paths |

Unsupported editable document cases include extended character rendering, printer-dependent formatting, complex packet references and protected/encrypted documents. There is no complete menu-by-menu inventory of the DOS program yet.

## Verification

R49 was run with 65C02, its matching ROM, and FAT32 SD images. The formatted smoke document was retrieved, edited and saved as a separate file on each profile. Test tools checked that the exported bytes matched across profiles. libwpd recognized the text, bold and underline as separate formatted spans.

The native storage diagnostic uses the same document module as the editor. It opens a synthetic formatted WP 5.x document from an emulated SD card, scans for a marker near the end, inserts text at the start and across an 8 KiB bank boundary, and writes a new document. A host check compares the **entire file** with the source plus the two insertions. The diagnostic scan measures a straight document scan, which is distinct from the interactive F2 workflow.

Early GUI responsiveness measurements used 40 synthetic single keypresses on a blank document and observed changes in the VM's X11 framebuffer. The 2 MHz run measured about 68 ms p95, above the requested 50 ms. It observed a frame change for all 40 injections. It did not test held-key repeat, interrupted I/O, long-paragraph reflow, page-move timing or end-to-end latency on hardware. Physical display, keyboard and SD behavior require hardware testing.

| X16Emu profile | Formatted body | Open | Stream scan | Save | Byte comparison |
|---|---:|---:|---:|---:|---|
| 2 MHz / 512 KiB | 128 KiB | 37.28 s | 28.45 s | 13.87 s | Passed |
| 8 MHz / 512 KiB | 128 KiB | 8.80 s | 6.58 s | 3.93 s | Passed |
| 8 MHz / 2 MiB | 512 KiB | 35.10 s | 26.30 s | 15.58 s | Passed |

Times are KERNAL ticks divided by 60. Save timing excludes the preceding gap relocation and insert operations. The three profiles used the same storage diagnostic binary.

### DOS exchange

WordPerfect 5.1 Plus opened the native editor's formatted fixture in DOSBox 0.74-3 and saved an additional character. The X16 document module then loaded that DOS-saved file from emulated SD, inserted another character and saved it. A byte comparison verified its entire 335-byte DOS prefix and every original formatting code. The returned file was retrieved and displayed by WordPerfect. An independent libwpd check verified the returned text and both attributes.

While testing that return path, WordPerfect inserted retrieved content into the nonempty document at the active cursor. X16Perfect currently replaces its document after a discard prompt, a confirmed muscle-memory gap. The test covered a small bold-and-underline document. It did not establish complex-document interoperability.

The release PRG rebuilt byte for byte from a clean build. SHA-256 is `fbec2ca06a77d2d451e463fafb7b206f45772f6cd201b4427f37aa6b050b0a69`.
