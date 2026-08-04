# Inkscape CDR Import Extension (bundled cdr2svg)

A self-contained Inkscape input extension for CorelDRAW (`.cdr`) and Corel
Presentation Exchange (`.cmx`) files. It vendors patched copies of
[libcdr](https://gerrit.libreoffice.org/admin/repos/libcdr) and
[librevenge](https://sourceforge.net/projects/libwpd/) — including local
improvements to text layout, multi-line pitch, bitmap alpha handling, font
style flags, and CDR X5+ solid fills that are not upstream — and compiles
them into a single small converter binary, `cdr2svg`. No system libcdr or
librevenge is needed at runtime; the only shared-library dependencies are
zlib, lcms2, and ICU.

## Layout

- `src/lib/cdr/` — vendored libcdr parser sources (patched)
- `src/lib/rvng/` — vendored librevenge subset: core types, streams, and the
  patched SVG drawing generator
- `src/inc/` — public headers for both, plus a small
  `librevenge-generators` shim header
- `src/conv/cdr2svg.cpp` — CLI entry point
- `cdr_input.inx`, `cmx_input.inx`, `cdr_input.py` — the Inkscape extension
- `Makefile` — primary build; `CMakeLists.txt` — alternative CMake build

## Building

Requires g++ (C++17), make, pkg-config, and development headers for zlib,
lcms2, ICU (`icu-i18n`/`icu-uc`), and Boost (headers only; used at build
time, not linked).

    make            # produces ./cdr2svg
    ./install.sh    # builds if needed, installs into
                    # ~/.config/inkscape/extensions/

## Using

Restart Inkscape after installing. In **File → Open**, choose the
**CorelDRAW (bundled cdr2svg) (*.cdr)** file type and open your file.

Note: if your Inkscape build has built-in CDR support (it links system
libcdr), that importer takes precedence when the file type is left on
"automatic". Select the *bundled cdr2svg* entry in the open dialog's
file-type list to use this extension instead.

### Multi-page documents

Page 1 is imported by default and a notice is shown if the document has
more pages. To import another page, set `INKSCAPE_CDR_PAGE=N` in the
environment before starting Inkscape, or convert manually:

    cdr2svg --pages file.cdr          # print page count
    cdr2svg --page 2 file.cdr > page2.svg

## Updating the vendored sources

After changing the sibling checkouts (`../libcdr`,
`../libwpd-librevenge`), run:

    ./update-vendor.sh && make && ./install.sh

## Licensing

libcdr is MPL 2.0; librevenge is LGPL 2.1 / MPL 2.0 dual-licensed. The
vendored copies retain their license headers; this extension as a whole is
distributed under MPL 2.0.
