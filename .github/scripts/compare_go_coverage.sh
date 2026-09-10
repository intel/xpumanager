#!/bin/bash
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#
# Compares the Go coverage of a base and head checkout, prints the result as a
# markdown report.
#
# Expects "coverage-{base,head}" directories (in CWD), each containing
# per-Go-module coverage profiles named "coverage-{hand-written,generated}.out"
# (see level-zero-go/hack/coverage-report.sh).

set -e -u -o pipefail

if [ $# -ne 3 ]; then
    echo "Usage: ${0##*/} <title> <go-module-prefix> <base-ref>" >&2
    exit 1
fi

TITLE=$1
MODULE_PREFIX=$2
BASE_REF=$3

SCRIPT_DIR="$(dirname "$(realpath "$0")")"

echo "## Coverage report: ${TITLE}"

for bucket in hand-written generated; do
    for target_rev in base head; do
        find "coverage-$target_rev" -name "coverage-$bucket.out" -print0 |
            xargs -0 python3 "$SCRIPT_DIR/go_cover_summary.py" --strip-prefix "$MODULE_PREFIX" \
            > "summary-$target_rev-$bucket.json"
    done

    echo
    # shellcheck disable=SC2016  # the backticks are literal markdown, not a command substitution
    python3 "$SCRIPT_DIR/compare_coverage.py" \
        "summary-base-$bucket.json" "summary-head-$bucket.json" \
        --base-ref "$BASE_REF" \
        --title "### ${bucket^} code" \
        --line-label 'Statement coverage' \
        --measured-by 'the Go unit tests (`make coverage`)'
done
