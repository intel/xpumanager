#!/bin/sh
# Copyright (C) 2025-2026 Intel Corporation
# SPDX-License-Identifier: MIT


set -e

# switch to project root
if [ -z "$MESON_SOURCE_ROOT" ] || [ -z "$MESON_BUILD_ROOT" ]; then
	echo "ERROR: MESON_SOURCE_ROOT / MESON_BUILD_ROOT not set!" 1>&2
	exit 1
fi
cd "$MESON_SOURCE_ROOT"

# Check if 'dot' tool is available for generating graphs
if [ -z $(which dot 2>/dev/null) ]; then
	echo "ERROR: 'dot' tool for generating graphs missing, please install 'graphviz' first!" 1>&2
	exit 1
fi

conf="Doxyfile"
if [ ! -f "$conf" ]; then
	echo "ERROR: Doxygen '$conf' config file missing!" 1>&2
	exit 1
fi

echo "Doxygen config ('$conf'):"
sed "s%^\(OUTPUT_DIRECTORY *=\).*$%\1 $MESON_BUILD_ROOT%" $conf
sed "s%^\(OUTPUT_DIRECTORY *=\).*$%\1 $MESON_BUILD_ROOT%" $conf | doxygen -
