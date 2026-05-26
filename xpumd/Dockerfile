#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

ARG BASE_IMAGE=scratch

# The build stage
FROM golang:1.26-trixie AS builder

ARG IGC_CORE_VERSION=2.34.4+21428
ARG COMPUTE_RUNTIME_RELEASE=26.18.38308.1
ARG LIBIGDGMM12_VERSION=22.10.0
ARG LEVEL_ZERO_VERSION=1.28.6
ARG IGSC_VERSION=0.9.6

WORKDIR /go/src/work

# Enable source packages and install IGSC build dependencies
RUN sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/debian.sources && \
    apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    libcyaml-dev \
    libyaml-dev \
    ninja-build \
    libudev-dev \
    pkg-config

# Download and install Level-Zero and its dependencies
RUN mkdir /debs /out && \
    IGC_CORE_RELEASE=${IGC_CORE_VERSION%%+*} && \
    curl -fS --fail-early --output-dir /debs/ \
         -LO https://github.com/intel/intel-graphics-compiler/releases/download/v${IGC_CORE_RELEASE}/intel-igc-core-2_${IGC_CORE_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libigdgmm12_${LIBIGDGMM12_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libze-intel-gpu1_${COMPUTE_RUNTIME_RELEASE}-0_amd64.deb \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze1_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze-dev_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb && \
    dpkg -i /debs/*.deb

# Build IGSC
RUN mkdir /src && \
    git clone --branch V${IGSC_VERSION} --depth 1 https://github.com/intel/igsc.git /src/igsc && \
    cd /src/igsc && \
    cmake -G Ninja -S . -B build -LH -DCMAKE_INSTALL_PREFIX=/usr/ && \
    ninja -v -C build && \
    ninja -C build install && \
    ldconfig

# Build xpumd
RUN --mount=type=cache,target=/go/pkg/mod/ \
    --mount=type=cache,target=/root/.cache/go-build \
    --mount=src=.,target=.,rw \
    make build && \
    make -C level-zero-go/level-zero-stub clean test && \
    install -D dist/xpumd /out/usr/local/bin/xpumd && \
    install -D dist/xpuinfo-cli /out/usr/local/bin/xpuinfo-cli && \
    install -D config-example.yaml /out/etc/xpumd/config-example.yaml && \
    install -D level-zero-go/level-zero-stub/libze_stub.so \
               /out/usr/local/lib/xpumd/level-zero-stub/libze_loader.so.1 && \
    install -D level-zero-go/level-zero-stub/example-config.yaml \
               /out/etc/xpumd/level-zero-stub/example-config.yaml && \
    go -C dist tool -modfile $(pwd)/tools/go.mod \
        go-licenses save . \
                --ignore github.com/intel/xpumanager/xpumd \
                --save_path /out/usr/share/doc/xpumd/licenses

# (L)GPL compliance: store source packages stored in /sources
# Copy licences for all installed packages
RUN mkdir /out/sources && \
    cd /out/sources && \
    gpl_pkgs="libc6 libcap2 libudev1 libstdc++6 libgcc-s1" && \
    apt-get source --download-only $gpl_pkgs && \
    other_pkgs="libigdgmm12 libze-intel-gpu1 libcyaml1 libyaml-0-2" && \
    for pkg in $gpl_pkgs $other_pkgs; do \
        install -Dt /out/usr/share/doc/$pkg \
                /usr/share/doc/$pkg/copyright; \
    done && \
    install -Dt /out/usr/share/doc/libigc-core-2 \
            /usr/local/lib/igc2/NOTICES.txt && \
    install -Dt /out/usr/share/common-licenses/ \
            /usr/share/common-licenses/*

# Copy dependencies for the final image
RUN install -Dt /out/lib64 \
            /lib64/ld-linux-x86-64.so.2 && \
    install -Dt /out/lib/x86_64-linux-gnu \
            /lib/x86_64-linux-gnu/libc.so.6 \
            /lib/x86_64-linux-gnu/libcap.so.2 \
            /lib/x86_64-linux-gnu/libcyaml.so.1 \
            /lib/x86_64-linux-gnu/libyaml-0.so.2 && \
    install -Dt /out/usr/lib/x86_64-linux-gnu \
            /usr/lib/x86_64-linux-gnu/libgcc_s.so.1 \
            /usr/lib/x86_64-linux-gnu/libm.so.6 \
            /usr/lib/x86_64-linux-gnu/libresolv.so.2 \
            /usr/lib/x86_64-linux-gnu/libstdc++.so.6 \
            /usr/lib/x86_64-linux-gnu/libudev.so.1 \
            /usr/lib/x86_64-linux-gnu/libigdgmm.so.12 \
            /usr/lib/x86_64-linux-gnu/libze_intel_gpu.so.1 \
            /usr/lib/x86_64-linux-gnu/libze_loader.so.1 \
            /usr/lib/x86_64-linux-gnu/libigsc.so.0 && \
    install -Dt /out/usr/local/lib \
            /usr/local/lib/libigc.so.2

# The final image
FROM ${BASE_IMAGE} AS final

COPY --from=builder /out/ /

USER 65534:65534

ENTRYPOINT ["/usr/local/bin/xpumd"]
