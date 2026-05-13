#!/bin/bash -ex
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

set -o pipefail

SCRIPT_DIR="$(dirname "$(realpath "$0")")"

OTEL_VERSION=0.152.0

cd "$SCRIPT_DIR/.."
sed "s/@OTEL_VERSION@/${OTEL_VERSION}/g" builder-config.yaml.in > builder-config.yaml
go tool -modfile tools/go.mod builder --config=builder-config.yaml --skip-compilation

for m in $(git ls-files '*/go.mod' | xargs dirname); do
    go -C "$m" mod download
done
