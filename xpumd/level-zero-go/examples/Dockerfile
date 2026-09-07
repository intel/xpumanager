#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

# Where the final image gets the Level-Zero GPU backend from:
# - "deb": the released libze-intel-gpu1 package (default)
# - "src": a build of the compute-runtime sources
ARG BACKEND=deb

# driver stack DEB package versions
ARG IGC_CORE_VERSION=2.40.13+22418
ARG COMPUTE_RUNTIME_RELEASE=26.31.39395.13
ARG LIBIGDGMM12_VERSION=22.10.0
ARG LEVEL_ZERO_VERSION=1.33.1

FROM golang:1.26-trixie AS builder

ARG APP=list-sysman

ARG IGC_CORE_VERSION
ARG COMPUTE_RUNTIME_RELEASE
ARG LIBIGDGMM12_VERSION
ARG LEVEL_ZERO_VERSION

# SHA256 checksums for those
ARG CHECKSUMS_LOADER=checksums-loader.txt
ARG CHECKSUMS_BACKEND=checksums-backend.txt

# Git repo tags & commit hashes to match against them
ARG IGSC_VERSION=V1.3.1
ARG IGSC_COMMIT=483f0b91d4636cbdc8f0503343f858ff2da45432
ARG METEE_VERSION=6.2.4
ARG METEE_COMMIT=2e9390294f47c3d88590cb4a6e0293e31fd4a9db

WORKDIR /debs/
COPY level-zero/$CHECKSUMS_LOADER level-zero/$CHECKSUMS_BACKEND ./

# Fetch & extract backend package contents (to be copied to final stage)
RUN IGC_CORE_RELEASE=${IGC_CORE_VERSION%%+*} && \
    curl -fS --fail-early --output-dir /debs/ \
         -LO https://github.com/intel/intel-graphics-compiler/releases/download/v${IGC_CORE_RELEASE}/intel-igc-core-2_${IGC_CORE_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libigdgmm12_${LIBIGDGMM12_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libze-intel-gpu1_${COMPUTE_RUNTIME_RELEASE}-0_amd64.deb \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze1_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb && \
    # The backend is extracted separately, it is picked up by the "backend" stage below
    for deb in /debs/*.deb; do \
        case $deb in \
            */libze-intel-gpu1_*) dpkg-deb -x "$deb" /backend ;; \
            *) dpkg-deb -x "$deb" /out ;; \
        esac; \
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
    cmake -G Ninja -Wno-dev -S . -B build -LH -DCMAKE_INSTALL_PREFIX=/usr/ && \
    ninja -v -C build && \
    ninja -C build install && \
    git clone --branch ${IGSC_VERSION} --depth 1 https://github.com/intel/igsc.git /src/igsc && \
    cd /src/igsc && git checkout ${IGSC_COMMIT} && \
    cmake -G Ninja -Wno-dev -S . -B build -LH -DCMAKE_INSTALL_PREFIX=/usr/local && \
    ninja -v -C build && \
    DESTDIR=/igsc-install ninja -C build install

# Build Sysman Go bindings $APP example
RUN --mount=type=cache,target=/go/pkg/mod/ \
    --mount=src=.,target=.,ro \
    cd examples && \
    make build-${APP} BIN_DIR=/out/usr/local/bin


# The backend from the released DEB package (BACKEND=deb)
FROM builder AS backend-deb


# The backend built from the compute-runtime sources (BACKEND=src)
#
# The sources are pinned to the revision that the Intel headers in include/intel/ are
# vendored from, and the dependencies to the versions that the compute-runtime
# manifests/manifest.yml pins for that revision. The build args of this stage are
# pinned by hack/vendor-intel-header.sh, together with the vendored headers, the ones
# at the top of this file by hand.
FROM golang:1.26-trixie AS backend-src

# Compute-runtime commit date 2026-08-25
ARG COMPUTE_RUNTIME_COMMIT=9c2abbc844661895dc8c45ad84cdc50eb19e146c
ARG LIBIGDGMM12_VERSION=22.10.0
ARG LIBIGDGMM12_COMMIT=0246660b2ade17afc1c9c4c510368fa649ca809f
ARG LEVEL_ZERO_VERSION=1.32.0

# The Graphics Compiler is NOT taken from the manifest to avoid compiling it from sources, too.
ARG IGC_CORE_VERSION=2.40.13+22418

# SHA256 checksums for the Graphics Compiler and the Level-Zero loader packages.
ARG CHECKSUMS_IGC=checksums-igc.txt
ARG CHECKSUMS_LOADER=checksums-loader-src.txt

WORKDIR /debs/
COPY level-zero/$CHECKSUMS_IGC level-zero/$CHECKSUMS_LOADER .

# The backend is built against the Level-Zero API headers of the loader release
RUN curl -fS --fail-early --output-dir /debs/ \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze1_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze-dev_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb && \
    if ! sha256sum --check --strict $CHECKSUMS_LOADER ; then \
        echo "--- Expected ($CHECKSUMS_LOADER): ---"; \
        cat $CHECKSUMS_LOADER; \
        echo "--- Got: ---"; \
        sha256sum *.deb; \
        echo "=> ERROR"; \
        exit 1; \
    fi && \
    dpkg -i /debs/*.deb && \
    rm -f /debs/*.deb

# The Graphics Compiler that the compute-runtime is built against.
# NOTE: the opencl packages are needed as build dependencies even though the OpenCL driver is not built.
RUN IGC_CORE_RELEASE=${IGC_CORE_VERSION%%+*} && \
    IGC_URL=https://github.com/intel/intel-graphics-compiler/releases/download/v${IGC_CORE_RELEASE} && \
    curl -fS --fail-early --output-dir /debs/ \
         -LO ${IGC_URL}/intel-igc-core-2_${IGC_CORE_VERSION}_amd64.deb \
         -LO ${IGC_URL}/intel-igc-core-devel_${IGC_CORE_VERSION}_amd64.deb \
         -LO ${IGC_URL}/intel-igc-opencl-2_${IGC_CORE_VERSION}_amd64.deb \
         -LO ${IGC_URL}/intel-igc-opencl-devel_${IGC_CORE_VERSION}_amd64.deb && \
    if ! sha256sum --check --strict $CHECKSUMS_IGC ; then \
        echo "--- Expected ($CHECKSUMS_IGC): ---"; \
        cat $CHECKSUMS_IGC; \
        echo "--- Got: ---"; \
        sha256sum *.deb; \
        echo "=> ERROR"; \
        exit 1; \
    fi && \
    dpkg -i /debs/*.deb && \
    ldconfig

# Install other compute-runtime build dependencies
# (without the libtracefs headers the info log extension is built as a stub)
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    libtracefs-dev \
    ninja-build

# GmmLib provides the graphics memory management library to build against.
# No devel package is released, so it is built from sources here
# (the final image uses it from the released libigdgmm12 package).
RUN SRC=/src/gmmlib && BUILD=$SRC/build && \
    git clone --branch intel-gmmlib-${LIBIGDGMM12_VERSION} --depth 1 https://github.com/intel/gmmlib.git $SRC && \
    git -C $SRC checkout ${LIBIGDGMM12_COMMIT} && \
    cmake -G Ninja -Wno-dev -LH -S $SRC -B $BUILD \
          -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_INSTALL_PREFIX=/usr/local && \
    ninja -C $BUILD && \
    ninja -C $BUILD install && \
    ldconfig

# Only the Level-Zero backend library is built, the OpenCL driver is of no interest here.
# ("cmake -LH" lists the cache variables, i.e. which platforms & features are supported)
RUN SRC=/src/compute-runtime && BUILD=$SRC/build && \
    git init -q $SRC && \
    git -C $SRC fetch -q --depth 1 \
        https://github.com/intel/compute-runtime.git ${COMPUTE_RUNTIME_COMMIT} && \
    git -C $SRC checkout -q FETCH_HEAD && \
    cmake -G Ninja -Wno-dev -LH -S $SRC -B $BUILD \
          -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_WITH_L0=1 \
          -DNEO_BUILD_WITH_OCL=0 \
          -DNEO_SKIP_UNIT_TESTS=1 && \
    ninja -C $BUILD ze_intel_gpu && \
    install -d /backend/usr/lib/x86_64-linux-gnu/ && \
    cp -P $BUILD/bin/libze_intel_gpu.so* /backend/usr/lib/x86_64-linux-gnu/


# The backend selected with BACKEND
FROM backend-${BACKEND} AS backend


# The final image
FROM ubuntu:24.04 AS final

ARG APP=list-sysman

# Copy $APP example, driver stack and its deps
COPY --from=builder /out/ /
COPY --from=backend /backend/ /
COPY --from=builder /igsc-install/usr/local/lib /usr/local/lib

# Install Sysman deps & strace tool from distro & symlink example to entrypoint
RUN apt-get update && \
    # Sysman does not link these libs but loads them at runtime: netlink for RAS &
    # fabric metrics, tracefs for reading CPER events/logs from the kernel
    apt-get install -y libnl-genl-3-200 libtracefs1 strace && \
    apt-get clean && rm -rf /var/lib/apt/lists/* && \
    ln -s /usr/local/bin/${APP} /entrypoint

CMD ["/entrypoint"]
