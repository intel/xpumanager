#!/bin/bash
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#
# Prints the Go coverage as a markdown report. With a base ref, compares the
# coverage of a base and head checkout, otherwise reports that of the head.
#
# Expects "coverage-head" and, if comparing, "coverage-base" directories (in
# CWD), each containing per-Go-module coverage profiles named
# "modules/<tree>/<module>/coverage-{hand-written,generated}.out", reported as
# module "<tree>/<module>" (see level-zero-go/hack/coverage-report.sh).

set -e -u -o pipefail

if [ $# -ne 2 ] && [ $# -ne 3 ]; then
    echo "Usage: ${0##*/} <title> <go-module-prefix> [<base-ref>]" >&2
    exit 1
fi

TITLE=$1
MODULE_PREFIX=$2
BASE_REF=${3:-}
# shellcheck disable=SC2016  # the backticks are literal markdown, not a command substitution
MEASURED_BY='the Go unit tests (`make coverage`) and integration tests (`make coverage-integration-test-kind`)'

SCRIPT_DIR="$(dirname "$(realpath "$0")")"

REVS=head
[ -n "$BASE_REF" ] && REVS="base head"

echo "## Coverage report: ${TITLE}"

for bucket in hand-written generated; do
    for target_rev in $REVS; do
        # Run in the modules directory, the directory of a profile is its module
        (cd "coverage-$target_rev/modules" &&
            find . -name "coverage-$bucket.out" -print0 |
            xargs -0 python3 "$SCRIPT_DIR/go_cover_summary.py" --strip-prefix "$MODULE_PREFIX") \
            > "summary-$target_rev-$bucket.json"
    done
    echo
    if [ -n "$BASE_REF" ]; then
        python3 "$SCRIPT_DIR/compare_coverage.py" \
            "summary-base-$bucket.json" "summary-head-$bucket.json" \
            --base-ref "$BASE_REF" \
            --title "### ${bucket^} code" \
            --line-label 'Statement coverage' \
            --measured-by "$MEASURED_BY"
    else
        python3 "$SCRIPT_DIR/report_coverage.py" "summary-head-$bucket.json" \
            --title "### ${bucket^} code" \
            --line-label 'Statement coverage' \
            --measured-by "$MEASURED_BY"
    fi
done
