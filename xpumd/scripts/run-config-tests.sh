#!/bin/bash
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT
#
# Config validation tests for the built xpumd binary, using its "validate" sub-command.
#
# The invalid configs are overlays merged on top of config-example.yaml.

set -u -o pipefail

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
TOPDIR="$(realpath "${SCRIPT_DIR}/..")"

XPUMD=${XPUMD:-${TOPDIR}/dist/xpumd}
EXAMPLE_CONFIG="${TOPDIR}/config-example.yaml"
CONFIG_TEST_DIR="${TOPDIR}/test/config"

if [ ! -x "${XPUMD}" ]; then
    echo "ERROR: xpumd binary '${XPUMD}' not found, Run 'make build', or point XPUMD to the binary."
    exit 1
fi

failed=0

pass () {
    echo "✓ $1"
}

fail () {
    echo "X $1"
    failed=1
}

echo -e "\nValidating configs with '${XPUMD}'...\n"

# Verify the example config is accepted
if out=$("${XPUMD}" validate --config="${EXAMPLE_CONFIG}" 2>&1); then
    pass "config-example.yaml: accepted"
else
    fail "config-example.yaml: should be valid but was rejected:" "${out}"
fi

# The invalid overlays must be rejected with the expected error.
for config in "${CONFIG_TEST_DIR}"/invalid-*.yaml; do
    name=${config##*/}

    expected=$(sed -n 's/^# *expect-error: *//p' "${config}")
    if [ -z "${expected}" ]; then
        fail "${name}: no '# expect-error:' comment in the file"
        continue
    fi

    if out=$("${XPUMD}" validate --config="${EXAMPLE_CONFIG}" --config="${config}" 2>&1); then
        fail "${name}: accepted, but should have been rejected with: ${expected}"
        continue
    fi
    if ! echo "${out}" | grep -qF -- "${expected}"; then
        fail "${name}: expected error to contain '${expected}', got: '${out}'"
        continue
    fi

    pass "${name}: rejected"
done

echo
if [ "${failed}" -ne 0 ]; then
    echo "=> FAIL"
    exit 1
fi
echo "✓ All config validation tests passed"
