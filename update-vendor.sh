#!/bin/sh
# Re-sync the vendored sources from the sibling library checkouts
# (../libcdr and ../libwpd-librevenge) after making changes there.
set -e
cd "$(dirname "$0")"

CDR=../libcdr
RVNG=../libwpd-librevenge

cp "$CDR"/src/lib/*.cpp "$CDR"/src/lib/*.h src/lib/cdr/
cp "$CDR"/inc/libcdr/*.h src/inc/libcdr/

for f in RVNGString RVNGStringVector RVNGProperty RVNGPropertyList \
         RVNGPropertyListVector RVNGBinaryData RVNGMemoryStream \
         RVNGStreamImplementation RVNGOLEStream RVNGZipStream \
         RVNGSVGDrawingGenerator; do
    cp "$RVNG/src/lib/$f.cpp" src/lib/rvng/
done
for h in RVNGMemoryStream RVNGOLEStream RVNGZipStream librevenge_internal; do
    cp "$RVNG/src/lib/$h.h" src/lib/rvng/
done
cp "$RVNG"/inc/librevenge/*.h src/inc/librevenge/
cp "$RVNG"/inc/librevenge-stream/*.h src/inc/librevenge-stream/

echo "Vendored sources updated. Run 'make' to rebuild."
