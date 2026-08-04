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

### Prebuilt releases

Tagged releases (`v*`) are built automatically for Linux (x86_64, arm64),
Windows (x86_64), and macOS (arm64, x86_64) by the GitHub Actions workflow
in `.github/workflows/release.yml`. Each zip contains the extension files
plus a self-contained `bin/cdr2svg` (static ICU/lcms2/zlib on Linux,
bundled DLLs on Windows, bundled dylibs on macOS); unpack its contents
into the Inkscape user extensions directory (see the included
`INSTALL.txt`). To cut a release:

    git tag v0.1.0 && git push origin v0.1.0

## Using

Restart Inkscape after installing. `.cdr` and `.cmx` files then open
directly via **File → Open**, drag-and-drop, or the command line.

The `.inx` files declare `priority="1"` on their `<input>` elements, which
makes Inkscape's autodetection prefer this extension over the built-in
libcdr importer (extensions with a non-zero priority sort ahead of the
zero-priority built-ins). Remove the attribute to restore the built-in
importer. Note the built-in still handles `.cdt` and `.ccx` files, which
this extension does not register.

### Multi-page documents

Page 1 is imported by default and a notice is shown if the document has
more pages. To import another page, set `INKSCAPE_CDR_PAGE=N` in the
environment before starting Inkscape, or convert manually:

    cdr2svg --pages file.cdr          # print page count
    cdr2svg --page 2 file.cdr > page2.svg

## Vendored source provenance

The vendored sources were last synced from these commits of the sibling
checkouts:

- `libcdr` @ `71d8270` — "Derive multi-line text pitch from Corel spacing
  and frame height." (6 local commits on top of upstream 0.1.8)
- `librevenge` @ `cc7b519d` — "Improve SVG text layout for multi-line CDR
  import." (1 local commit on top of upstream)

## Updating the vendored sources

After changing the sibling checkouts (`../libcdr`,
`../libwpd-librevenge`), run:

    ./update-vendor.sh && make && ./install.sh

and update the commit hashes listed above.

## Licensing

libcdr is MPL 2.0; librevenge is LGPL 2.1 / MPL 2.0 dual-licensed. The
vendored copies retain their license headers; this extension as a whole is
distributed under MPL 2.0.
