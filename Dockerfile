#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: Apache-2.0

ARG BASE_IMAGE=ubuntu:24.04

# The build stage
FROM golang:1.25-trixie AS builder

ARG IGC_CORE_VERSION=2.27.10+20617
ARG COMPUTE_RUNTIME_RELEASE=26.01.36711.4
ARG LIBIGDGMM12_VERSION=22.9.0
ARG LEVEL_ZERO_VERSION=1.26.3
ARG IGSC_VERSION=0.9.6

WORKDIR /go/src/work

# Install IGSC build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    libudev-dev

# Download and install Level-Zero and its dependencies
RUN mkdir /debs /out && \
    IGC_CORE_RELEASE=${IGC_CORE_VERSION%%+*} && \
    curl -fS --fail-early --output-dir /debs/ \
         -LO https://github.com/intel/intel-graphics-compiler/releases/download/v${IGC_CORE_RELEASE}/intel-igc-core-2_${IGC_CORE_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libigdgmm12_${LIBIGDGMM12_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libze-intel-gpu1_${COMPUTE_RUNTIME_RELEASE}-0_amd64.deb \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/level-zero_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb && \
    for deb in /debs/*.deb; do \
        dpkg-deb -x "$deb" /out; \
    done

RUN curl -fS --fail-early --output-dir /debs \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/level-zero-devel_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb && \
    dpkg -i /debs/*.deb

# Build IGSC
RUN mkdir /src && \
    git clone --branch V${IGSC_VERSION} --depth 1 https://github.com/intel/igsc.git /src/igsc && \
    cd /src/igsc && \
    cmake -G Ninja -S . -B build -LH -DCMAKE_INSTALL_PREFIX=/usr/local && \
    ninja -v -C build && \
    DESTDIR=/igsc-install ninja -C build install

# Build xpumd
RUN --mount=type=cache,target=/go/pkg/mod/ \
    --mount=type=cache,target=/root/.cache/go-build \
    --mount=src=.,target=.,rw \
    make build && \
    install -D dist/xpumd /out/usr/local/bin/xpumd && \
    install -D dist/xpuinfo-cli /out/usr/local/bin/xpuinfo-cli && \
    install -D config-example.yaml /out/etc/xpumd/config-example.yaml && \
    go -C dist tool -modfile $(pwd)/tools/go.mod \
        go-licenses save . \
                --ignore github.com/intel/xpumanager \
                --save_path /out/usr/share/doc/xpumd/licenses

# The final image
FROM ${BASE_IMAGE} AS minimal

COPY --from=builder /out/ /
COPY --from=builder /igsc-install/usr/local/lib /usr/local/lib

# Sysman does not link netlink lib but loads it at runtime, for RAS & fabric metrics
RUN apt-get update && \
    apt-get install -y --no-install-recommends libnl-genl-3-200 && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

RUN ldconfig

USER 65534:65534

ENTRYPOINT ["/usr/local/bin/xpumd"]
