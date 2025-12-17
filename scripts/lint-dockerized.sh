#!/bin/bash -ex

LEVEL_ZERO_VERSION=${LEVEL_ZERO_VERSION:-"1.26.0"}
L0_BASE_URL="https://github.com/oneapi-src/level-zero/releases/download"

docker run --rm -v $(pwd):/app -w /app \
    golang:1.25 \
    /bin/sh -c \
    "curl -LO ${L0_BASE_URL}/v${LEVEL_ZERO_VERSION}/level-zero_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb \
          -LO ${L0_BASE_URL}/v${LEVEL_ZERO_VERSION}/level-zero-devel_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb && \
     apt-get install -y ./*.deb && \
     make lint"
