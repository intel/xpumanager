FROM amr-registry.caas.intel.com/cache/library/ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install system dependencies
RUN apt-get update && \
    apt-get install -y \
        build-essential \
        ccache \
        meson \
        ninja-build \
        cmake \
        pkg-config \
        python3-pip \
        python3-venv \
        clang-tidy \
        clang-format \
        valgrind \
        lcov \
        gcovr \
        git \
        doxygen \
        libpciaccess-dev \
        libudev-dev && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Install Conan
RUN python3 -m pip install --break-system-packages conan>=2.0

# Create Conan profile directory and create our predefined profile
RUN mkdir -p ~/.conan2/profiles && \
    echo '[settings]' > ~/.conan2/profiles/default && \
    echo 'arch=x86_64' >> ~/.conan2/profiles/default && \
    echo 'build_type=Release' >> ~/.conan2/profiles/default && \
    echo 'compiler=gcc' >> ~/.conan2/profiles/default && \
    echo 'compiler.cppstd=20' >> ~/.conan2/profiles/default && \
    echo 'compiler.libcxx=libstdc++11' >> ~/.conan2/profiles/default && \
    echo 'compiler.version=13' >> ~/.conan2/profiles/default && \
    echo 'os=Linux' >> ~/.conan2/profiles/default && \
    echo '' >> ~/.conan2/profiles/default && \
    echo '[conf]' >> ~/.conan2/profiles/default && \
    echo 'tools.system.package_manager:mode=install' >> ~/.conan2/profiles/default && \
    echo 'tools.system.package_manager:sudo=True' >> ~/.conan2/profiles/default && \
    echo '' >> ~/.conan2/profiles/default && \
    echo '[buildenv]' >> ~/.conan2/profiles/default && \
    echo 'CC=gcc' >> ~/.conan2/profiles/default && \
    echo 'CXX=g++' >> ~/.conan2/profiles/default

# Also create it as the build profile (Conan 2.x needs both host and build profiles)
RUN cp ~/.conan2/profiles/default ~/.conan2/profiles/build

# Verify profiles were created correctly
RUN echo "Verifying Conan profiles..." && \
    ls -la ~/.conan2/profiles/ && \
    echo "Default profile contents:" && \
    cat ~/.conan2/profiles/default

# Set up Conan remotes (add common remotes)
RUN conan remote add conancenter --force https://center.conan.io

# Create a temporary directory to pre-install dependencies
WORKDIR /tmp/conan-deps

# Copy conanfile.py to pre-install dependencies
COPY conanfile.py .
COPY recipes/ recipes/

RUN mkdir -p /conan
ENV CONAN_USER_HOME=/conan

# Pre-install Conan dependencies for both Debug and Release
RUN conan create recipes/level-zero --name=level-zero --version=1.23.1 && \
    conan create recipes/metee --name=metee --version=6.0.0 && \
    conan create recipes/igsc --name=igsc --version=0.9.6 && \
    conan install . --build=missing -s build_type=Release && \
    conan install . --build=missing -s build_type=Debug

# Make the Conan cache globally accessible
RUN chmod -R 755 ~/.conan2

# Clean up temporary files but keep the Conan cache
RUN rm -rf /tmp/conan-deps

# Create a working directory for builds
WORKDIR /workspace

# Default command
CMD ["/bin/bash"]
