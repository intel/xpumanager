#!/bin/bash
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#
# Helper for running Go tests with coverage for multiple modules.
#
# Usage: go-test-coverage.sh <coverage-dir> <module>...
#
# Environment:
#   GO_TEST_FLAGS - extra flags for "go test"

set -e -u -o pipefail

GO_TEST_FLAGS="${GO_TEST_FLAGS:-}"

if [ $# -lt 2 ]; then
    echo "Usage: ${0##*/} <coverage-dir> <module>..."
    exit 1
fi

# The tests run in the module directories thus absolute path
COVERAGE_DIR=$(realpath -m "$1")
shift

error=0

for module in "$@"; do
    echo "Running tests in module '${module}'..."

    profile="${COVERAGE_DIR}/${module}/coverage.out"
    mkdir -p "$(dirname "${profile}")"

    # comma-separated list of packages given to -coverpkg, to cover also module dependencies without tests.
    if ! packages=$(go -C "${module}" list ./... | paste -sd,); then
        error=1
        continue
    fi

    # -count=1 to prevent caching of test results, needed because of the
    # stub driver, as the Go test cache misses (at least) the following things:
    # 1. PKG_CONFIG_PATH trick (a change in the stub driver is not tracked)
    # 2. the testdata/ files are read by the stub driver (not the Go code)
    # shellcheck disable=SC2086  # GO_TEST_FLAGS is a list of flags
    go -C "${module}" test ${GO_TEST_FLAGS} -count=1 \
       -coverpkg="${packages}" -coverprofile="${profile}" ./... || error=1

    # The last line of the report is the total coverage of the module. Capture
    # the report so that a failure of "go tool cover" is not masked by "tail".
    if summary=$(go -C "${module}" tool cover -func="${profile}"); then
        echo "${summary}" | tail -1
    else
        error=1
    fi
done

exit "${error}"
