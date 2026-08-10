#!/usr/bin/env python3
"""Inkscape input extension: import CorelDRAW (CDR/CMX) files.

Runs the bundled cdr2svg converter (built from patched libcdr/librevenge
sources) and writes the resulting SVG to stdout, which Inkscape reads.

Every page of the document is imported. Multi-page documents come in as an
Inkscape multi-page drawing (one <inkscape:page> per CorelDRAW page, laid
out left to right), which needs Inkscape 1.2 or later; older versions show
page 1 and keep the rest of the content on the canvas beside it.

To import a single page instead, set the environment variable
INKSCAPE_CDR_PAGE (1-based) before starting Inkscape, or run cdr2svg
manually with --page N.
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

    page = os.environ.get("INKSCAPE_CDR_PAGE")
    args = [converter, "--page", page] if page else [converter, "--all"]

    try:
        result = subprocess.run(
            args + [infile],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except OSError as exc:
        sys.stderr.write("cdr_input: failed to run %s: %s\n" % (converter, exc))
        sys.exit(1)

    if result.returncode != 0:
        sys.stderr.write(result.stderr.decode("utf-8", "replace"))
        sys.exit(result.returncode)

    # Anything on stderr makes Inkscape raise a "Script Error" dialog over the
    # imported drawing, so stay quiet unless the conversion actually failed.
    sys.stdout.buffer.write(result.stdout)


if __name__ == "__main__":
    main()
