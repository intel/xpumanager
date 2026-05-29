#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

FROM golang:1.26-trixie AS builder

ARG APP=list-sysman

# DEB package versions
ARG IGC_CORE_VERSION=2.34.4+21428
ARG COMPUTE_RUNTIME_RELEASE=26.18.38308.1
ARG LIBIGDGMM12_VERSION=22.10.0
ARG LEVEL_ZERO_VERSION=1.28.6

# Git repo tags & matching commit hashes
ARG IGSC_VERSION=V0.9.6
ARG IGSC_COMMIT=6abae21408bc36561baaade9fca9ae4a5173d2f0
ARG METEE_VERSION=6.2.3
ARG METEE_COMMIT=67ff323aead437557d9a7309803aae79cd5b2d56

WORKDIR /go/src/work

RUN mkdir /debs && \
    IGC_CORE_RELEASE=${IGC_CORE_VERSION%%+*} && \
    curl -fS --fail-early --output-dir /debs/ \
         -LO https://github.com/intel/intel-graphics-compiler/releases/download/v${IGC_CORE_RELEASE}/intel-igc-core-2_${IGC_CORE_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libigdgmm12_${LIBIGDGMM12_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libze-intel-gpu1_${COMPUTE_RUNTIME_RELEASE}-0_amd64.deb \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze1_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb && \
    for deb in /debs/*.deb; do \
        dpkg-deb -x "$deb" /out; \
    done

RUN curl -fS --output-dir /debs/ \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze-dev_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb && \
    dpkg -i /debs/*.deb

# Install IGSC build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    libudev-dev

# Build IGSC + its MeTee dependency
# (with --depth 1, clone $TAG + checkout $COMMIT verifies that tags & hashes match)
RUN mkdir /src && \
    git clone --branch ${METEE_VERSION} --depth 1 https://github.com/intel/metee.git /src/metee && \
    cd /src/metee && git checkout ${METEE_COMMIT} && \
    cmake -G Ninja -S . -B build -LH -DCMAKE_INSTALL_PREFIX=/usr/ && \
    ninja -v -C build && \
    ninja -C build install && \
    git clone --branch ${IGSC_VERSION} --depth 1 https://github.com/intel/igsc.git /src/igsc && \
    cd /src/igsc && git checkout ${IGSC_COMMIT} && \
    cmake -G Ninja -S . -B build -LH -DCMAKE_INSTALL_PREFIX=/usr/local && \
    ninja -v -C build && \
    DESTDIR=/igsc-install ninja -C build install

RUN --mount=type=cache,target=/go/pkg/mod/ \
    --mount=src=.,target=.,ro \
    cd examples/${APP} && \
    go build -o /out/usr/local/bin/ .


# The final image
FROM ubuntu:24.04 AS final

ARG APP=list-sysman

COPY --from=builder /out/ /
COPY --from=builder /igsc-install/usr/local/lib /usr/local/lib
RUN apt-get update && \
    # Sysman does not link netlink lib but loads it at runtime, for RAS & fabric metrics
    apt-get install -y libnl-genl-3-200 && \
    apt-get clean && rm -rf /var/lib/apt/lists/* && \
    ln -s /usr/local/bin/${APP} /entrypoint

CMD ["/entrypoint"]
