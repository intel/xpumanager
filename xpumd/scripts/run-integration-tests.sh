#!/bin/bash
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

SCRIPT_DIR="$(dirname "$(realpath "$0")")"

TIMEOUT=${TIMEOUT:-10m}

exec go -C "${SCRIPT_DIR}/../test/integration" test -count=1 -v \
    -timeout "${TIMEOUT}" ./k8s -args "$@"
