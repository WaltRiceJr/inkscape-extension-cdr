#!/usr/bin/env python3
"""Inkscape input extension: import CorelDRAW (CDR/CMX) files.

Runs the bundled cdr2svg converter (built from patched libcdr/librevenge
sources) and writes the resulting SVG to stdout, which Inkscape reads.

Page selection: page 1 is imported by default. For multi-page documents,
set the environment variable INKSCAPE_CDR_PAGE (1-based) before starting
Inkscape, or run cdr2svg manually with --page N.
"""

import os
import subprocess
import sys


def find_converter():
    here = os.path.dirname(os.path.abspath(__file__))
    exe = "cdr2svg.exe" if sys.platform.startswith("win") else "cdr2svg"
    candidates = [os.path.join(here, exe), os.path.join(here, "bin", exe)]
    for path in candidates:
        if os.path.isfile(path) and os.access(path, os.X_OK):
            return path
    # Fall back to PATH
    from shutil import which
    found = which(exe)
    if found:
        return found
    sys.stderr.write(
        "cdr_input: converter binary '%s' not found next to the extension.\n"
        "Build it with 'make' in the extension source directory and re-run "
        "install.sh.\n" % exe
    )
    sys.exit(1)


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: cdr_input.py INPUTFILE\n")
        sys.exit(1)
    infile = sys.argv[-1]
    converter = find_converter()

    page = os.environ.get("INKSCAPE_CDR_PAGE", "1")

    try:
        result = subprocess.run(
            [converter, "--page", page, infile],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except OSError as exc:
        sys.stderr.write("cdr_input: failed to run %s: %s\n" % (converter, exc))
        sys.exit(1)

    if result.returncode != 0:
        sys.stderr.write(result.stderr.decode("utf-8", "replace"))
        sys.exit(result.returncode)

    # Warn (non-fatally) when the document has more pages than we imported.
    try:
        pages = subprocess.run(
            [converter, "--pages", infile],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
        )
        count = int(pages.stdout.strip() or 1)
        if count > 1:
            sys.stderr.write(
                "Note: document has %d pages; imported page %s. Set "
                "INKSCAPE_CDR_PAGE to import a different page.\n" % (count, page)
            )
    except (ValueError, OSError):
        pass

    sys.stdout.buffer.write(result.stdout)


if __name__ == "__main__":
    main()
