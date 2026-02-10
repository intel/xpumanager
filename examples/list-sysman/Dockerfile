#
# Copyright (C) 2025 Intel Corporation
#
# SPDX-License-Identifier: MIT

FROM golang:1.25-trixie AS builder

ARG IGC_CORE_VERSION=2.27.10+20617
ARG COMPUTE_RUNTIME_RELEASE=26.01.36711.4
ARG LIBIGDGMM12_VERSION=22.9.0
ARG LEVEL_ZERO_VERSION=1.26.3
ARG IGSC_VERSION=0.9.6

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

# Install IGSC build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    libudev-dev

# Build IGSC
RUN mkdir /src && \
    git clone --branch V${IGSC_VERSION} --depth 1 https://github.com/intel/igsc.git /src/igsc && \
    cd /src/igsc && \
    cmake -G Ninja -S . -B build -LH -DCMAKE_INSTALL_PREFIX=/usr/local && \
    ninja -v -C build && \
    DESTDIR=/igsc-install ninja -C build install

RUN --mount=type=cache,target=/go/pkg/mod/ \
    --mount=src=.,target=.,ro \
    cd examples/list-sysman && \
    go build -o /out/ .


# The final image
FROM ubuntu:24.04 AS final

COPY --from=builder /debs /debs
COPY --from=builder /igsc-install/usr/local/lib /usr/local/lib
RUN apt-get update && \
    # Sysman does not link netlink lib but loads it at runtime, for RAS & fabric metrics
    apt-get install -y /debs/*.deb libnl-genl-3-200 && \
    rm -rf /debs && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

COPY --from=builder /out/* /usr/local/bin/

CMD ["/usr/local/bin/list-sysman"]
