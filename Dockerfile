#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

# The build stage
FROM golang:1.25-trixie AS builder

ARG IGC_CORE_VERSION=2.24.8+20344
ARG COMPUTE_RUNTIME_RELEASE=25.48.36300.8
ARG LIBIGDGMM12_VERSION=22.8.2
ARG LEVEL_ZERO_VERSION=1.26.2

WORKDIR /go/src/work

RUN mkdir /debs && \
    IGC_CORE_RELEASE=${IGC_CORE_VERSION%%+*} && \
    curl -LO https://github.com/intel/intel-graphics-compiler/releases/download/v${IGC_CORE_RELEASE}/intel-igc-core-2_${IGC_CORE_VERSION}_amd64.deb --output-dir /debs/ && \
    curl -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libigdgmm12_${LIBIGDGMM12_VERSION}_amd64.deb --output-dir /debs/ && \
    curl -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libze-intel-gpu1_${COMPUTE_RUNTIME_RELEASE}-0_amd64.deb --output-dir /debs/ && \
    curl -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/level-zero_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb --output-dir /debs/ && \
    apt-get update && apt-get install -y /debs/*.deb

RUN mkdir /debs-devel && \
    curl -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/level-zero-devel_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb --output-dir /debs-devel/ && \
    apt-get install -y /debs-devel/*.deb

RUN --mount=type=cache,target=/go/pkg/mod/ \
    --mount=src=.,target=.,ro \
    make build OUT_DIR=/out


# The final image
FROM ubuntu:24.04 AS final

COPY --from=builder /debs /debs
RUN apt-get update && \
    apt-get install -y /debs/*.deb && \
    rm -rf /debs && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

COPY --from=builder /out/* /usr/local/bin/

USER 65534:65534

CMD ["/usr/local/bin/xpu-exporter"]
