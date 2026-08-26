#!/bin/bash -e
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

# Copy the default build args of the "backend-src" stage from another Dockerfile,
# together with the comment recording the date of the pinned compute-runtime revision.
# Ensures that both build the compute-runtime backend from the same sources.

# We assume the script is run from the root of the repository
SRC=level-zero-go/examples/Dockerfile
DST=Dockerfile

TMP=$(mktemp)

awk '
    FNR == 1 { pass++; stage = "" }
    /^FROM / { stage = ($0 ~ / AS backend-src$/) ? "src" : "other" }
    # Dropped here and re-emitted with the commit that it describes
    stage == "src" && /^# Compute-runtime commit date / {
        if (pass == 1) { date_comment = $0 }
        next
    }
    stage == "src" && /^ARG [A-Z0-9_]+=/ {
        split($2, arg, "=")
        if (pass == 1) { value[arg[1]] = arg[2]; next }
        if (arg[1] in value) {
            if (arg[1] == "COMPUTE_RUNTIME_COMMIT" && date_comment != "") { print date_comment }
            print "ARG " arg[1] "=" value[arg[1]]
            next
        }
    }
    pass == 2
' "$SRC" "$DST" > "$TMP"

echo "Updating the ARGS of $DST..."
# Written in place to keep the permissions of the file
cat "$TMP" > "$DST"
rm -f "$TMP"
