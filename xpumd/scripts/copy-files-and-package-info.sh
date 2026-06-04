#!/bin/bash
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
set -euxo pipefail

if [ $# -lt 2 ]; then
    cat >&2 << EOF
Usage: $0 <OUT_DIR> <file> [<file> ...]"

Purposed to run in the context of a Docker build only.
EOF
    exit 1
fi

OUT="$1"
shift

# Copy files, preserving their absolute path
for f in "$@"; do
    install -D "$f" "$OUT/$f"
done

# (L)GPL compliance: source archives and copyright notices
mkdir -p "$OUT/sources"

# Discover DEB packages that own the copied files. Fail on any unowned file.
PKGS=$(realpath "$@" | xargs dpkg -S | sed 's/:.*//' | sort -u)

# Copy copyright/license info, and sources for packages not flagged as non-(L)GPL.
for pkg in $PKGS; do
    case "$pkg" in
        # intel-igc-core-2 ships a NOTICES.txt instead of a copyright file
        intel-igc-core-2)
            install -Dt "$OUT/usr/share/doc/$pkg" \
                    /usr/local/lib/igc2/NOTICES.txt
            ;;
        # libze1 (github release deb) currently ships no /usr/share/doc
        # (MIT-licensed but no file to copy), tolerate missing copyright file
        libze1)
            install -Dt "$OUT/usr/share/doc/$pkg" \
                    /usr/share/doc/$pkg/copyright 2>/dev/null || true
            ;;
        # Non-(L)GPL packages: copyright notice only
        libigdgmm12|libze-intel-gpu1|libcyaml1|libyaml-0-2)
            install -Dt "$OUT/usr/share/doc/$pkg" \
                    /usr/share/doc/$pkg/copyright
            ;;
        # Default: treat as (L)GPL, download source archive and copy copyright
        *)
            ( cd "$OUT/sources" && apt-get source --download-only "$pkg" )
            install -Dt "$OUT/usr/share/doc/$pkg" \
                    /usr/share/doc/$pkg/copyright
            ;;
    esac
done

# Copy common license headers.
install -Dt "$OUT/usr/share/common-licenses/" \
        /usr/share/common-licenses/*

# Copy OS identity for scanners to detect the origin (and e.g. select the right CVE database).
install -D /etc/os-release "$OUT/etc/os-release"
install -D /etc/debian_version "$OUT/etc/debian_version"

# Construct a dpkg database for scanners
STATUS="$OUT/var/lib/dpkg/status"
mkdir -p "$(dirname "$STATUS")"

for pkg in $PKGS; do
    dpkg-query -s "$pkg" >> "$STATUS"
    echo >> "$STATUS"
done
