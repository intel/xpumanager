#!/bin/bash -ex

THIS_DIR="$(dirname "$(realpath "$0")")"

LEVEL_ZERO_VERSION=${LEVEL_ZERO_VERSION:-"1.26.2"}
L0_BASE_URL="https://github.com/oneapi-src/level-zero/releases/download"

IMAGE_TAG="xpumd-builder:latest"

docker build -t "${IMAGE_TAG}" - <<EOF
FROM golang:1.25
RUN apt-get update && apt-get install -y --no-install-recommends unzip && \
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

docker run --rm \
    ${USER_FLAG} \
    -e HOME=/go \
    -v "${THIS_DIR}/..:/go/src" \
    -w /go/src \
    "${IMAGE_TAG}" \
    /bin/sh -c "make $*"
