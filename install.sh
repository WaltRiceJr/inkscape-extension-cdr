#!/bin/sh
# Install the CDR import extension into the user's Inkscape extensions
# directory. Builds cdr2svg first if it isn't built yet.
set -e

cd "$(dirname "$0")"

if [ ! -x cdr2svg ]; then
    echo "Building cdr2svg..."
    make -j"$(nproc 2>/dev/null || echo 2)"
fi

DEST="${XDG_CONFIG_HOME:-$HOME/.config}/inkscape/extensions"
mkdir -p "$DEST"

install -m 755 cdr2svg "$DEST/"
install -m 755 cdr_input.py "$DEST/"
install -m 644 cdr_input.inx cmx_input.inx "$DEST/"

echo "Installed to $DEST"
echo "Restart Inkscape; .cdr/.cmx files can now be opened via"
echo "'CorelDRAW (bundled cdr2svg)' in the file-open dialog."
