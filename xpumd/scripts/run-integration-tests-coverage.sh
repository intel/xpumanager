#!/bin/bash
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

set -e -u -o pipefail

SCRIPT_DIR="$(dirname "$(realpath "$0")")"

if [ $# -lt 2 ]; then
    echo "Usage: ${0##*/} <coverage-data-dir> <coverage-profile> [test args]..." >&2
    exit 1
fi

COVERAGE_DATA_DIR=$1
COVERAGE_PROFILE=$2
shift 2

XPUMD_PKG=$(go -C "${SCRIPT_DIR}" list -m)

rm -rf "${COVERAGE_DATA_DIR}" "${COVERAGE_PROFILE}"
mkdir -p "${COVERAGE_DATA_DIR}"

TIMEOUT=${TIMEOUT:-20m} "${SCRIPT_DIR}/run-integration-tests.sh" --coverage-dir="${COVERAGE_DATA_DIR}" "$@"

go tool covdata textfmt -i="${COVERAGE_DATA_DIR}" -o="${COVERAGE_PROFILE}"
# Drop the module's generated dist/ entrypoint. It's ephemeral auto-generated
# code just for the build step (not in git, and its import path does not map
# back to any source files for the report tools).
sed -i "\|^${XPUMD_PKG}/[^/]*\.go:|d" "${COVERAGE_PROFILE}"

echo
echo "Integration test coverage of the instrumented packages:"
go tool covdata percent -i="${COVERAGE_DATA_DIR}" | sort
