#!/bin/sh
#
# Run all relevant static checkers
#
# TODO: add more linters

retcode=0
# all tools present?
for cmd in codespell shellcheck; do
	if ! which "$cmd" >/dev/null 2>&1; then
		echo "ERROR: '$cmd' lint tool missing!" 1>&2
		retcode=$((retcode+1))
	fi
done
if [ $retcode -gt 0 ]; then
	exit $retcode
fi

# exit on first error
set -e

# switch to project root
if [ -z "$MESON_SOURCE_ROOT" ]; then
	echo "ERROR: MESON_SOURCE_ROOT not set!" 1>&2
	exit 1
fi
cd "$MESON_SOURCE_ROOT"

# run static checkers for...

# code spelling: ignore binary files, PCI vendor names, 3rd party code & auto-generated files
codespell --ignore-words-list renderD,som,SOM  --skip '*.exe,*.gz,*.pdf,*.xlsx,pci.ids,third_party,html,meson-*'

# shell script best practices
find . -name '*.sh' -print0 | xargs -0 shellcheck
