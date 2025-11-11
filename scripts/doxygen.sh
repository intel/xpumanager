#!/bin/sh

set -e

# switch to project root
if [ -z "$MESON_SOURCE_ROOT" ] || [ -z "$MESON_BUILD_ROOT" ]; then
	echo "ERROR: MESON_SOURCE_ROOT / MESON_BUILD_ROOT not set!" 1>&2
	exit 1
fi
cd "$MESON_SOURCE_ROOT"

conf="Doxyfile"
if [ ! -f "$conf" ]; then
	echo "ERROR: Doxygen '$conf' config file missing!" 1>&2
	exit 1
fi

echo "Doxygen config ('$conf'):"
sed "s%^\(OUTPUT_DIRECTORY *=\).*$%\1 $MESON_BUILD_ROOT%" $conf
sed "s%^\(OUTPUT_DIRECTORY *=\).*$%\1 $MESON_BUILD_ROOT%" $conf | doxygen -
