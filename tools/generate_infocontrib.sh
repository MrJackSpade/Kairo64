#!/bin/sh
# Writes core/generated/infocontrib.h, VICE's credits (info.c), from third_party/vice/doc/vice.texi
# with VICE's own script, as its Makefile does at build time; --check compares instead. The script
# writes scratch files to the current directory, so it runs in a temporary one.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
src="$root/third_party/vice/src"
out="$root/core/generated/infocontrib.h"
year=$(grep "VICEDATE_YEAR " "$src/vicedate.h" | cut -d " " -f 3)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
(cd "$work" && sh "$src/buildtools/geninfocontrib_h.sh" infocontrib.h "$year" < "$root/third_party/vice/doc/vice.texi") \
    | LC_ALL=C sed -f "$src/buildtools/infocontrib.sed" | iconv -f ISO-8859-15 -t UTF-8 > "$work/infocontrib.h"
if [ "$1" = "--check" ]; then
    if ! cmp -s "$work/infocontrib.h" "$out"; then
        echo "core/generated/infocontrib.h is out of date: run tools/generate_infocontrib.sh" >&2
        exit 1
    fi
else
    cp "$work/infocontrib.h" "$out"
fi
