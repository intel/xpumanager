#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

# The build stage
FROM golang:1.25-trixie AS builder

ARG IGC_CORE_VERSION=2.27.10+20617
ARG COMPUTE_RUNTIME_RELEASE=26.01.36711.4
ARG LIBIGDGMM12_VERSION=22.9.0
ARG LEVEL_ZERO_VERSION=1.26.3

WORKDIR /go/src/work

RUN mkdir /debs /out && \
    IGC_CORE_RELEASE=${IGC_CORE_VERSION%%+*} && \
    curl -LO https://github.com/intel/intel-graphics-compiler/releases/download/v${IGC_CORE_RELEASE}/intel-igc-core-2_${IGC_CORE_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libigdgmm12_${LIBIGDGMM12_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libze-intel-gpu1_${COMPUTE_RUNTIME_RELEASE}-0_amd64.deb \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/level-zero_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb --output-dir /debs/ && \
    for deb in /debs/*.deb; do \
        dpkg-deb -x "$deb" /out; \
    done

RUN curl -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/level-zero-devel_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb --output-dir /debs && \
    dpkg -i /debs/*.deb

RUN --mount=type=cache,target=/go/pkg/mod/ \
    --mount=type=cache,target=/root/.cache/go-build \
    --mount=src=.,target=.,rw \
    make build && \
    install -D dist/xpumd /out/usr/local/bin/xpumd && \
    install -D dist/xpuinfo-cli /out/usr/local/bin/xpuinfo-cli && \
    install -D config-example.yaml /out/etc/xpumd/config-example.yaml


# The final image
FROM ubuntu:24.04 AS final

COPY --from=builder /out/ /

USER 65534:65534

ENTRYPOINT ["/usr/local/bin/xpumd"]
