#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

ARG BASE_IMAGE=scratch

# The build stage
FROM golang:1.26-trixie AS builder

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

# Enable source packages, install IGSC build dependencies and extra Sysman dependencies
RUN sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/debian.sources && \
    apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    libcyaml-dev \
    libyaml-dev \
    ninja-build \
    libudev-dev \
    pkg-config \
    libnl-genl-3-200

WORKDIR /debs/
COPY level-zero-go/level-zero/$CHECKSUMS_LOADER level-zero-go/level-zero/$CHECKSUMS_BACKEND ./

# Download and install Level-Zero and its dependencies
RUN IGC_CORE_RELEASE=${IGC_CORE_VERSION%%+*} && \
    curl -fS --fail-early \
         -LO https://github.com/intel/intel-graphics-compiler/releases/download/v${IGC_CORE_RELEASE}/intel-igc-core-2_${IGC_CORE_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libigdgmm12_${LIBIGDGMM12_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libze-intel-gpu1_${COMPUTE_RUNTIME_RELEASE}-0_amd64.deb \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze1_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze-dev_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb && \
    if ! sha256sum --check --strict $CHECKSUMS_LOADER $CHECKSUMS_BACKEND ; then \
        echo "--- Expected ($CHECKSUMS_LOADER / $CHECKSUMS_BACKEND): ---"; \
        cat $CHECKSUMS_LOADER $CHECKSUMS_BACKEND; \
        echo "--- Got: ---"; \
        sha256sum *.deb; \
        echo "=> ERROR"; \
        exit 1; \
    fi && \
    dpkg -i *.deb

# Copy runtime libraries and their package info.
# Placed here so that this layer is cached (avoid downloading of deb source packages, in particular).
COPY scripts/copy-files-and-package-info.sh .
RUN ./copy-files-and-package-info.sh /out \
        /lib64/ld-linux-x86-64.so.2 \
        /usr/lib/x86_64-linux-gnu/libc.so.6 \
        /usr/lib/x86_64-linux-gnu/libcap.so.2 \
        /usr/lib/x86_64-linux-gnu/libcyaml.so.1 \
        /usr/lib/x86_64-linux-gnu/libyaml-0.so.2 \
        /usr/lib/x86_64-linux-gnu/libgcc_s.so.1 \
        /usr/lib/x86_64-linux-gnu/libm.so.6 \
        /usr/lib/x86_64-linux-gnu/libnl-3.so.200 \
        /usr/lib/x86_64-linux-gnu/libnl-genl-3.so.200 \
        /usr/lib/x86_64-linux-gnu/libresolv.so.2 \
        /usr/lib/x86_64-linux-gnu/libstdc++.so.6 \
        /usr/lib/x86_64-linux-gnu/libudev.so.1 \
        /usr/lib/x86_64-linux-gnu/libigdgmm.so.12 \
        /usr/lib/x86_64-linux-gnu/libze_intel_gpu.so.1 \
        /usr/lib/x86_64-linux-gnu/libze_loader.so.1 \
        /usr/local/lib/libigc.so.2

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
    cmake -G Ninja -S . -B build -LH -DCMAKE_INSTALL_PREFIX=/usr/ && \
    ninja -v -C build && \
    ninja -C build install && \
    ldconfig

# libigsc and libmetee are source-built (not DEB packages): copy manually and add synthetic dpkg status entries for scanners
RUN install -D /usr/lib/x86_64-linux-gnu/libigsc.so.1 \
               /out/usr/lib/x86_64-linux-gnu/libigsc.so.1 && \
    /usr/bin/echo -e "\
Package: libigsc1\n\
Status: install ok installed\n\
Architecture: amd64\n\
Version: ${IGSC_VERSION#V}\n\
Homepage: https://github.com/intel/igsc\n\
Description: Intel(R) Graphics System Controller Firmware Update Library\n\
\n\
Package: libmetee0\n\
Status: install ok installed\n\
Architecture: amd64\n\
Version: ${METEE_VERSION}\n\
Homepage: https://github.com/intel/metee\n\
Description: Intel(R) ME TEE Library (statically linked to libigsc)\n\
\n" >> /out/var/lib/dpkg/status

# Build xpumd / xpuinfo-cli + stub driver & install them
RUN --mount=type=cache,target=/go/pkg/mod/ \
    --mount=type=cache,target=/root/.cache/go-build \
    --mount=src=.,target=.,rw \
    mkdir -p /out && \
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

# Endpoints cannot be limited to localhost in a container,
# as then accessing them would require using host networking
RUN sed -i 's/127.0.0.1:/0.0.0.0:/' /out/etc/xpumd/config-example.yaml

# The final image
FROM ${BASE_IMAGE} AS final

COPY --from=builder /out/ /

USER 65534:65534

# default "intel_xpu_info" socket dir
ENV XDG_RUNTIME_DIR=/run/xpumd

ENTRYPOINT ["/usr/local/bin/xpumd"]
