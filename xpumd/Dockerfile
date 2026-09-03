#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

# Use '--build-arg TOOLS="strace"' to build local XPUMD image that includes specified debugging tools
# - works only for tools that do not need configs etc, only their libs
# Then run image with other entrypoint than the default XPUMD one, e.g:
# - "docker run -it --rm --cap-add SYS_PTRACE --entrypoint strace ... <image> -f /usr/local/bin/xpumd"
ARG TOOLS=""

ARG BASE_IMAGE=scratch

# BACKEND sets where the final image gets the Level-Zero driver stack from:
# - "deb": downloads driver packages from their GitHub project pages & builds IGSC + MeTee (default)
# - "src": like "deb", but builds also "libze-intel-gpu1" library from the compute-runtime sources
#   (needed for the experimental Sysman extensions not supported (yet) by the driver packages)
# - "local": uses locally downloaded DEB packages from FILES_DIR directory for whole driver stack.
#    Requires also suitable FILES_DIR variable (see debs/docker-build.sh):
#      FILES_DIR=debs/<package-dir>
ARG BACKEND=deb


# --- Base build stage with necessary distro setup / packages ---
FROM golang:1.26-trixie AS builder-base

# Enable source packages, install IGSC build dependencies and extra Sysman dependencies
# (Sysman does not link the latter but loads them at runtime: netlink for RAS and
# fabric metrics, tracefs for info logs)
RUN sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/debian.sources && \
    apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    libcyaml-dev \
    libyaml-dev \
    ninja-build \
    libudev-dev \
    pkg-config \
    libnl-genl-3-200 \
    libtracefs1

WORKDIR /debs/

COPY scripts/copy-files-and-package-info.sh .


# --- Build stage for using local driver DEBs ---
FROM builder-base AS builder-local

# User needs to override this
ARG FILES_DIR="unspecified-FILES_DIR"

# All driver stack packages, including IGSC + MeTee
COPY $FILES_DIR/*.deb .

# Install them & show compiler lib names & versions
RUN dpkg -i ./*.deb  &&  ls -l /usr/lib/x86_64-linux-gnu/libig*.so.*


# --- Build stage for downloading driver DEBs ---
#
# IGSC + MeTee libraries are built to get a specific version of them
# (they are not available from their project's releases pages, and
# Intel Kobuk PPA driver repo offers only single _rolling_ version)
FROM builder-base AS builder-deb

# driver stack DEB package versions
ARG IGC_CORE_VERSION=2.40.13+22418
ARG COMPUTE_RUNTIME_RELEASE=26.31.39395.13
ARG LIBIGDGMM12_VERSION=22.10.0
ARG LEVEL_ZERO_VERSION=1.33.1

# Git repo tags & commit hashes to match against them for IGSC source build
ARG IGSC_VERSION=V1.3.1
ARG IGSC_COMMIT=483f0b91d4636cbdc8f0503343f858ff2da45432
ARG METEE_VERSION=6.2.4
ARG METEE_COMMIT=2e9390294f47c3d88590cb4a6e0293e31fd4a9db

# SHA256 checksums for packages
ARG CHECKSUMS_LOADER=checksums-loader.txt
ARG CHECKSUMS_BACKEND=checksums-backend.txt

ARG FILES_DIR=level-zero-go/level-zero
COPY $FILES_DIR/$CHECKSUMS_LOADER $FILES_DIR/$CHECKSUMS_BACKEND ./

# Download and install Level-Zero and its dependencies.
RUN IGC_CORE_RELEASE=${IGC_CORE_VERSION%%+*} && \
    curl -fS --fail-early \
         -LO https://github.com/intel/intel-graphics-compiler/releases/download/v${IGC_CORE_RELEASE}/intel-igc-core-2_${IGC_CORE_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libigdgmm12_${LIBIGDGMM12_VERSION}_amd64.deb \
         -LO https://github.com/intel/compute-runtime/releases/download/${COMPUTE_RUNTIME_RELEASE}/libze-intel-gpu1_${COMPUTE_RUNTIME_RELEASE}-0_amd64.deb \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze1_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze-dev_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb \
	 && \
    if ! sha256sum --check --strict $CHECKSUMS_LOADER $CHECKSUMS_BACKEND ; then \
        echo "--- Expected ($CHECKSUMS_LOADER / $CHECKSUMS_BACKEND): ---"; \
        cat $CHECKSUMS_LOADER $CHECKSUMS_BACKEND; \
        echo "--- Got: ---"; \
        sha256sum ./*.deb; \
        echo "=> ERROR"; \
        exit 1; \
    fi && \
    dpkg -i ./*.deb

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
    cmake -G Ninja -Wno-dev -S . -B build -LH -DCMAKE_INSTALL_PREFIX=/usr/ && \
    ninja -v -C build && \
    ninja -C build install && \
    ldconfig

# libigsc and libmetee are source-built (not DEB packages):
# copy manually to output dir and add synthetic dpkg status entries for scanners
RUN install -D /usr/lib/x86_64-linux-gnu/libigsc.so.1 \
               /out/usr/lib/x86_64-linux-gnu/libigsc.so.1 && \
    install -d /out/var/lib/dpkg/ && \
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


# --- Alias for source build builder stage ---
FROM builder-deb AS builder-src


# --- Final build stage ---
#
# Copy dependencies + build XPUMD binaries required on the final image to a suitable location
FROM builder-${BACKEND} AS builder

ARG BACKEND

# Copy runtime libraries and their package info:
# - Placed here so that this layer is cached (avoid downloading of deb source packages, in particular)
# - The GPU backend library is not here, the selected "backend" stage provides it with
#   the package info matching that build
# - Some IGC packages install compute compiler to incorrect (/usr/local) location
# - IGSC package can be either v0 or v1 version, but Sysman can load/use at runtime either version
# - MeTee version (IGSC dep) in repos differs a lot, and its package does not provide *.so.[0-9] symlink
RUN /debs/copy-files-and-package-info.sh /out \
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
        /usr/lib/x86_64-linux-gnu/libtraceevent.so.1 \
        /usr/lib/x86_64-linux-gnu/libtracefs.so.1 \
        /usr/lib/x86_64-linux-gnu/libudev.so.1 \
        /usr/lib/x86_64-linux-gnu/libigdgmm.so.12 \
        /usr/lib/x86_64-linux-gnu/libze_loader.so.1 \
      $(if [ -e /usr/local/lib/libigc.so.2 ]; then \
          echo /usr/local/lib/libigc.so.2; \
	else \
          echo /usr/lib/x86_64-linux-gnu/libigc.so.2; \
        fi) \
      $(if [ "$BACKEND" = "local" ]; then \
          echo \
            /usr/lib/x86_64-linux-gnu/libigsc.so.[01] \
            /lib/x86_64-linux-gnu/libmetee.so.*.*.* \
        ; fi)

# Install debug tool binary & its libs to where they get copied to final image
ARG TOOLS
RUN if [ -n "$TOOLS" ]; then \
      apt-get update && \
      apt-get install -y --no-install-recommends $TOOLS && \
      /debs/copy-files-and-package-info.sh /out \
      $(for pkg in $TOOLS; do \
        for bin in $(dpkg -L $pkg | grep bin/); do \
          echo $bin; \
          ldd $bin | awk '/=>/{lib=$3; if(lib=="not") lib="missing:"$1; print lib}'; \
        done \
      done); \
    fi

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


# --- Backend from the released DEB package (BACKEND=deb) ---
FROM builder AS backend-deb

RUN /debs/copy-files-and-package-info.sh /backend \
        /usr/lib/x86_64-linux-gnu/libze_intel_gpu.so.1


# --- Alias for local DEB package usage (BACKEND=local) ---
FROM backend-deb AS backend-local


# --- The backend built from the compute-runtime sources (BACKEND=src) ---
#
# The sources are pinned to the revision that the Intel headers in
# level-zero-go/include/intel/ are vendored from, and the dependencies to the versions
# that the compute-runtime manifests/manifest.yml pins for that revision. The build
# args of this stage are pinned by level-zero-go/hack/vendor-intel-header.sh, in
# level-zero-go/examples/Dockerfile, and copied over from there by
# scripts/sync-dockerfile-args.sh.
FROM builder-base AS backend-src

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
COPY level-zero-go/level-zero/$CHECKSUMS_IGC level-zero-go/level-zero/$CHECKSUMS_LOADER ./

# The backend is built against the Level-Zero API headers of the loader release
RUN curl -fS --fail-early --output-dir /debs/ \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze1_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb \
         -LO https://github.com/oneapi-src/level-zero/releases/download/v${LEVEL_ZERO_VERSION}/libze-dev_${LEVEL_ZERO_VERSION}+u24.04_amd64.deb && \
    if ! sha256sum --check --strict $CHECKSUMS_LOADER ; then \
        echo "--- Expected ($CHECKSUMS_LOADER): ---"; \
        cat $CHECKSUMS_LOADER; \
        echo "--- Got: ---"; \
        sha256sum ./*.deb; \
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
        sha256sum ./*.deb; \
        echo "=> ERROR"; \
        exit 1; \
    fi && \
    dpkg -i /debs/*.deb && \
    ldconfig

# Install other compute-runtime build dependencies
# - libtracefs-dev: needed by Sysman for the info log extension functionality
# - libudev-dev: needed by Sysman for the (udev) events (e.g. device attach/detach)
RUN apt-get update && apt-get install -y --no-install-recommends \
    libtracefs-dev libudev-dev

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

# License and package info of the build for the final image.
# We create a synthetic dpkg status entry with the driver version of the build
# and the revision it was built from.
RUN install -D /src/compute-runtime/LICENSE.md \
               /backend/usr/share/doc/libze-intel-gpu1/copyright && \
    VERSION_H=/src/compute-runtime/build/driver_version.h && \
    VERSION=$(sed -n 's/^#define NEO_OCL_DRIVER_VERSION "\(.*\)"$/\1/p' $VERSION_H) && \
    if [ -z "$VERSION" ]; then \
        echo "--- $VERSION_H: ---"; \
        cat $VERSION_H; \
        echo "=> ERROR: no NEO_OCL_DRIVER_VERSION to describe the backend with"; \
        exit 1; \
    fi && \
    mkdir -p /backend/var/lib/dpkg/ && \
    /usr/bin/echo -e "\
Package: libze-intel-gpu1\n\
Status: install ok installed\n\
Architecture: amd64\n\
Version: ${VERSION}+git.${COMPUTE_RUNTIME_COMMIT}\n\
Homepage: https://github.com/intel/compute-runtime\n\
Description: Intel(R) Graphics Compute Runtime for oneAPI Level Zero\n\
 Built from the pinned compute-runtime revision, not from the released package.\n\
\n" > /backend/var/lib/dpkg/status


# --- The backend selected with BACKEND ---
FROM backend-${BACKEND} AS backend


# --- The image content, with the selected backend and its package info ---
FROM builder AS out

# Only the files of the backend: its dpkg status entry is appended to the database
# of the builder instead of replacing it
COPY --from=backend /backend/usr/ /out/usr/
COPY --from=backend /backend/var/lib/dpkg/status /backend-package-info
RUN cat /backend-package-info >> /out/var/lib/dpkg/status


# --- The final image ---
FROM ${BASE_IMAGE} AS final

COPY --from=out /out/ /

USER 65534:65534

# default "intel_xpu_info" socket dir
ENV XDG_RUNTIME_DIR=/run/xpumd

ENTRYPOINT ["/usr/local/bin/xpumd"]
