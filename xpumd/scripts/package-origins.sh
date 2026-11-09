#!/bin/bash
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
set -euo pipefail

if [ $# -lt 2 ]; then
    cat >&2 << EOF
Usage: $0 init <FILE>
       $0 add <FILE> <package> <purl> <download-location>
       $0 merge <FILE> <other-FILE> [<package> ...]

Maintains an SPDX JSON document of the image packages that do not come from the
distro repos.

init:  create FILE without any packages if it does not exist.
add:   add <package> (dpkg package name) or replace its entry.
merge: add or replace the packages of <other-FILE> (only the given ones, if any).
EOF
    exit 1
fi

CMD="$1"
FILE="$2"
shift 2

# Set the documentNamespace of <file> unique to its packages content
finalize() {
    local hash
    hash=$(jq -c .packages "$1" | sha256sum | cut -d' ' -f1)
    jq --arg ns "https://github.com/intel/xpumanager/xpumd/package-origins-$hash" \
        '.documentNamespace = $ns' "$1" > "$1.tmp"
    mv "$1.tmp" "$1"
}

# Create FILE without any packages, if it does not exist
init() {
    [ ! -e "$FILE" ] || return 0
    mkdir -p "$(dirname "$FILE")"
    jq -n --arg created "$(date -u +%Y-%m-%dT%H:%M:%SZ)" '{
        spdxVersion: "SPDX-2.3",
        dataLicense: "CC0-1.0",
        SPDXID: "SPDXRef-DOCUMENT",
        name: "xpumd-package-origins",
        documentNamespace: null,
        creationInfo: {created: $created, creators: ["Tool: package-origins.sh"]},
        packages: []
    }' > "$FILE"
    finalize "$FILE"
}

case "$CMD" in
    init)
        [ $# -eq 0 ] || { echo "ERROR: init takes no extra arguments" >&2; exit 1; }
        init
        exit 0
        ;;
    add)
        [ $# -eq 3 ] || { echo "ERROR: add needs <package> <purl> <download-location>" >&2; exit 1; }
        new=$(jq -n --arg name "$1" --arg purl "$2" --arg dl "$3" '
            (if ($dl | startswith("git+")) then "vcs_url" else "download_url" end) as $uri_type
            | {
                name: $name,
                SPDXID: ("SPDXRef-Package-" + ($name | gsub("[^A-Za-z0-9.-]"; "-"))),
                downloadLocation: $dl,
                filesAnalyzed: false,
                externalRefs: [{
                    referenceCategory: "PACKAGE-MANAGER",
                    referenceType: "purl",
                    referenceLocator: "\($purl)?\($uri_type)=\($dl | @uri)"
                }]
            } | [.]')
        ;;
    merge)
        [ $# -ge 1 ] || { echo "ERROR: merge needs <other-FILE>" >&2; exit 1; }
        new=$(jq '.packages | if $ARGS.positional == [] then . else
                      map(select(.name | IN($ARGS.positional[]))) end' "$1" --args "${@:2}")
        ;;
    *)
        echo "ERROR: unknown command '$CMD'" >&2
        exit 1
        ;;
esac

init
jq --argjson new "$new" '
    .packages |= map(select(.name | IN($new[].name) | not)) + $new
' "$FILE" > "$FILE.tmp"
mv "$FILE.tmp" "$FILE"
finalize "$FILE"
