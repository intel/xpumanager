#!/bin/bash
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
set -euo pipefail

if [ $# -lt 2 ]; then
    cat >&2 << EOF
Usage: $0 <OUT_FILE> <URL>[,purl=<purl>] [<URL>[,purl=<purl>] ...]

Downloads DEB packages to the current directory and records their origin for
the image SBOM to OUT_FILE (see copy-files-and-package-info.sh). The purl
is derived from GitHub release URLs, ",purl=<purl>" overrides it.

Purposed to run in the context of a Docker build only.
EOF
    exit 1
fi

DIR="$(dirname "$(realpath "$0")")"
OUT="$1"
shift

for arg in "$@"; do
    url="${arg%%,purl=*}"  # the download URL
    purl="${arg#"$url"}"   # optional comma-separated purl override
    purl="${purl#,purl=}"
    if [ -z "$purl" ]; then
        # We only support GitHub release assets (for now), match:
        # https://github.com/<owner>/<repo>/releases/download/<tag>/<asset>
        if [[ "$url" =~ ^https://github\.com/([^/]+)/([^/]+)/releases/download/([^/]+)/ ]]; then
            purl="pkg:github/${BASH_REMATCH[1]}/${BASH_REMATCH[2]}@${BASH_REMATCH[3]}"
        else
            echo "ERROR: cannot derive purl from '$url', append ,purl=<purl>" >&2
            exit 1
        fi
    fi
    curl -fS -LO "$url"
    pkg=$(dpkg-deb -f "$(basename "$url")" Package)
    "$DIR/package-origins.sh" add "$OUT" "$pkg" "$purl" "$url"
done
