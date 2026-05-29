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
CHECKSUMS=checksums-loader.txt

LEVEL_ZERO_VERSION_DEFAULT=$(sed -n 's/^ARG LEVEL_ZERO_VERSION=//p' "${SCRIPT_DIR}/../Dockerfile")
LEVEL_ZERO_VERSION=${LEVEL_ZERO_VERSION:-"${LEVEL_ZERO_VERSION_DEFAULT}"}
L0_BASE_URL="https://github.com/oneapi-src/level-zero/releases/download"

IMAGE_TAG="xpumd-builder:latest"

docker build -t "${IMAGE_TAG}" -f - "${SCRIPT_DIR}/../level-zero-go/level-zero" <<EOF
FROM golang:1.26
# Pre-create /go/pkg with wide permissions to allow "docker run" below (with non-root user)
# to write to it (the mount (-v) in docker run would otherwise create it with 755)
RUN mkdir -p /go/pkg && chmod 777 /go/pkg

COPY $CHECKSUMS .

# Unzip needed for installing protoc, doxygen for generating Go bindings
RUN apt-get update && apt-get install -y --no-install-recommends \
        unzip doxygen libyaml-dev libcyaml-dev jq clang-format && \
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
