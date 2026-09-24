#!/bin/bash -ex
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

# Args: XPUMD make target(s) to build in a container
#
# Note: does not work with SELinux/Podman (user privileges issue)

SCRIPT_DIR="$(dirname "$(realpath "$0")")"

# checksum file for L0 loader DEB packages
if [ "$BACKEND" = "src" ]; then
	CHECKSUMS=checksums-loader-src.txt
else
	CHECKSUMS=checksums-loader.txt
fi

DOCKERFILE="${SCRIPT_DIR}/../Dockerfile"
GET_VERSION='s/^ARG LEVEL_ZERO_VERSION=//p'

# The first version in the Dockerfile (of its build stage) is the default
LEVEL_ZERO_VERSION=$(sed -n "$GET_VERSION" "$DOCKERFILE" | head -n1)

# The backend stage that BACKEND selects (same arg as the image build) may pin its own
BACKEND_VERSION=$(sed -n "/^FROM .* backend-${BACKEND:-deb}\$/,/^FROM /{$GET_VERSION}" "$DOCKERFILE" | head -n1)
if [ -n "$BACKEND_VERSION" ]; then
    LEVEL_ZERO_VERSION=$BACKEND_VERSION
fi

L0_BASE_URL="https://github.com/oneapi-src/level-zero/releases/download"

IMAGE_TAG="xpumd-builder:latest"

docker build -t "${IMAGE_TAG}" -f - "${SCRIPT_DIR}/../level-zero-go/level-zero" <<EOF
FROM golang:1.26
# Pre-create /go/pkg with wide permissions to allow "docker run" below (with non-root user)
# to write to it (the mount (-v) in docker run would otherwise create it with 755)
RUN mkdir -p /go/pkg && chmod 777 /go/pkg

COPY $CHECKSUMS .

# Unzip needed for installing protoc, libyaml-dev/libcyaml-dev/jq/clang-format for generating Go bindings
RUN apt-get update && apt-get install -y --no-install-recommends \
        unzip libyaml-dev libcyaml-dev jq clang-format codespell shellcheck && \
    curl -LO ${L0_BASE_URL}/v${LEVEL_ZERO_VERSION}/libze1_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb \
         -LO ${L0_BASE_URL}/v${LEVEL_ZERO_VERSION}/libze-dev_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb && \
    if ! sha256sum --check $CHECKSUMS ; then \
	echo "--- Expected ($CHECKSUMS): ---"; \
	cat "$CHECKSUMS"; \
	echo "--- Got: ---"; \
	sha256sum *.deb; \
	echo "=> ERROR"; \
	exit 1; \
    fi; \
    dpkg -i ./*.deb && \
    rm -f ./*.deb && rm -rf /var/lib/apt/lists/*
EOF

if [ "$(ps -p 1 -o comm=)" = "systemd" ]; then
    USER="$(id -u):$(id -g)"
else
    echo "Probably running inside a container, running container as root"
    USER="0"
fi

GOMODCACHE=$(go env GOMODCACHE 2>/dev/null || true)
GOMODCACHE_MOUNT=()
if [ -n "${GOMODCACHE}" ] && [ -d "${GOMODCACHE}" ]; then
    echo "Mounting GOMODCACHE from host: ${GOMODCACHE}"
    GOMODCACHE_MOUNT=(-v "${GOMODCACHE}:/go/pkg/mod")
fi

docker run --rm \
    --user "${USER}" \
    "${GOMODCACHE_MOUNT[@]}" \
    -e HOME=/go \
    -v "${SCRIPT_DIR}/../..:/go/src" \
    -w /go/src \
    "${IMAGE_TAG}" \
    /bin/sh -c "make -C xpumd $*"
