#!/bin/bash -ex
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

set -o pipefail

SCRIPT_DIR="$(dirname "$(realpath "$0")")"

cd "$SCRIPT_DIR/.."

if [ -z "$XPUMD_VERSION" ]; then
	XPUMD_VERSION=$(git describe --tags --dirty --always | tr / -)
fi
OTEL_VERSION="$(sed -n 's/^OTEL_VERSION ?= *//p' Makefile)"

sed -e "s/@OTEL_VERSION@/${OTEL_VERSION}/" -e "s/@XPUMD_VERSION@/${XPUMD_VERSION}/" builder-config.yaml.in |
    go tool -modfile tools/go.mod builder --config=/dev/stdin --skip-compilation

for m in $(git ls-files '*/go.mod' | xargs dirname); do
    go -C "$m" mod download
done
