#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

FROM golang:1.26-trixie AS builder

ARG APP=list-sysman

# driver stack DEB package versions
ARG IGC_CORE_VERSION=2.38.2+22051
ARG COMPUTE_RUNTIME_RELEASE=26.27.39122.11
ARG LIBIGDGMM12_VERSION=22.10.0
ARG LEVEL_ZERO_VERSION=1.32.0

# SHA256 checksums for those
ARG CHECKSUMS_LOADER=checksums-loader.txt
ARG CHECKSUMS_BACKEND=checksums-backend.txt

# Git repo tags & commit hashes to match against them
ARG IGSC_VERSION=V1.3.1
ARG IGSC_COMMIT=483f0b91d4636cbdc8f0503343f858ff2da45432
ARG METEE_VERSION=6.2.4
ARG METEE_COMMIT=2e9390294f47c3d88590cb4a6e0293e31fd4a9db

WORKDIR /debs/
COPY level-zero/$CHECKSUMS_LOADER level-zero/$CHECKSUMS_BACKEND .

# Fetch & extract backend package contents (to be copied to final stage)
RUN IGC_CORE_RELEASE=${IGC_CORE_VERSION%%+*} && \
    curl -fS --fail-early --output-dir /debs/ \
         -LO https://github.com/intel/intel-graphics-compiler/releases/download/v${IGC_CORE_RELEASE}/intel-igc-core-2_${IGC_CORE_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libigdgmm12_${LIBIGDGMM12_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libze-intel-gpu1_${COMPUTE_RUNTIME_RELEASE}-0_amd64.deb \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze1_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb && \
    for deb in /debs/*.deb; do \
        dpkg-deb -x "$deb" /out; \
    done

# Fetch frontend package, and verify & install all packages (for building bindings examples)
RUN curl -fS --output-dir /debs/ \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze-dev_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb && \
    if ! sha256sum --check --strict $CHECKSUMS_LOADER $CHECKSUMS_BACKEND ; then \
        echo "--- Expected ($CHECKSUMS_LOADER / $CHECKSUMS_BACKEND): ---"; \
        cat $CHECKSUMS_LOADER $CHECKSUMS_BACKEND; \
        echo "--- Got: ---"; \
        sha256sum *.deb; \
        echo "=> ERROR"; \
        exit 1; \
    fi && \
    dpkg -i /debs/*.deb

# Install IGSC build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    libudev-dev

WORKDIR /go/src/work

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

# Build Sysman Go bindings $APP example
RUN --mount=type=cache,target=/go/pkg/mod/ \
    --mount=src=.,target=.,ro \
    cd examples/${APP} && \
    go build -o /out/usr/local/bin/ .


# The final image
FROM ubuntu:24.04 AS final

ARG APP=list-sysman

# Copy $APP example, driver stack and its deps
COPY --from=builder /out/ /
COPY --from=builder /igsc-install/usr/local/lib /usr/local/lib

# Install Sysman distro dep & symlink example to entrypoint
RUN apt-get update && \
    # Sysman does not link netlink lib but loads it at runtime, for RAS & fabric metrics
    apt-get install -y libnl-genl-3-200 && \
    apt-get clean && rm -rf /var/lib/apt/lists/* && \
    ln -s /usr/local/bin/${APP} /entrypoint

CMD ["/entrypoint"]
