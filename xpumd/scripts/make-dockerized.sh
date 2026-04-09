#!/bin/bash -ex
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT


SCRIPT_DIR="$(dirname "$(realpath "$0")")"

LEVEL_ZERO_VERSION=${LEVEL_ZERO_VERSION:-"1.26.2"}
L0_BASE_URL="https://github.com/oneapi-src/level-zero/releases/download"

IMAGE_TAG="xpumd-builder:latest"

docker build -t "${IMAGE_TAG}" - <<EOF
FROM golang:1.25
# Pre-create /go/pkg with wide permissions to allow "docker run" below (with non-root user)
# to write to it (the mount (-v) in docker run would otherwise create it with 755)
RUN mkdir -p /go/pkg && chmod 777 /go/pkg

# Unzip needed for installing protoc, doxygen for generating Go bindings
RUN apt-get update && apt-get install -y --no-install-recommends \
        unzip doxygen libcyaml-dev && \
    curl -LO ${L0_BASE_URL}/v${LEVEL_ZERO_VERSION}/level-zero_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb \
         -LO ${L0_BASE_URL}/v${LEVEL_ZERO_VERSION}/level-zero-devel_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb && \
    dpkg -i ./*.deb && \
    rm -f ./*.deb && rm -rf /var/lib/apt/lists/*
EOF

if [ "$(ps -p 1 -o comm=)" = "systemd" ]; then
    USER_FLAG="--user $(id -u):$(id -g)"
else
    echo "Probably running inside a container, running container as root"
fi

GOMODCACHE=$(go env GOMODCACHE 2>/dev/null || true)
GOMODCACHE_MOUNT=()
if [ -n "${GOMODCACHE}" -a -d "${GOMODCACHE}" ]; then
    echo "Mounting GOMODCACHE from host: ${GOMODCACHE}"
    GOMODCACHE_MOUNT=(-v "${GOMODCACHE}:/go/pkg/mod")
fi

docker run --rm \
    ${USER_FLAG} \
    "${GOMODCACHE_MOUNT[@]}" \
    -e HOME=/go \
    -v "${SCRIPT_DIR}/../..:/go/src" \
    -w /go/src \
    "${IMAGE_TAG}" \
    /bin/sh -c "make -C xpumd $*"
