#!/bin/bash
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

SCRIPT_DIR="$(dirname "$(realpath "$0")")"

TIMEOUT=${TIMEOUT:-10m}

# RUN_REGEX can be used to select which tests to run, e.g. RUN_REGEX=TestMetrics
RUN_REGEX=${RUN_REGEX:-.}

exec go -C "${SCRIPT_DIR}/../test/integration" test -count=1 -v \
    -timeout "${TIMEOUT}" -run "${RUN_REGEX}" ./k8s -args "$@"
