<!---

Copyright (C) 2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Building XPUM

This document describes how to build XPUM using the Conan-based build system.

## Table of Contents

- [Build with Conan](#recommended-build-with-conan-automated-dependencies)
  - [Prerequisites](#prerequisites)
  - [Install Conan](#install-conan)
  - [Configure Conan profile (first time only)](#configure-conan-profile-first-time-only)
  - [Create Conan recipes (first time only)](#create-conan-recipes-first-time-only)
  - [Build steps (using Conan)](#build-steps-using-conan)
    - [Linux](#linux)
    - [Windows](#windows)
  - [Debug Build (using Conan)](#debug-build-using-conan)

## Recommended: Build with Conan (Automated Dependencies)

### Prerequisites

- Python 3.8+
- Conan 2.0+
- Meson 1.10+
- Ninja
- C++ compiler with C++20 support
  - **Linux:** GCC 13+ or Clang 16+
  - **Windows:** Visual Studio 2022 (MSVC v143, i.e. `compiler.version=194`) with the "Desktop development with C++" workload
- Linux only:
  - If not using Conan, the following packages are needed at compile-time:
    - `libhwloc15`
    - `libcurl8.5`
    - `libudev-dev`
    - `libpciaccess-dev`
  - The following runtime tools are also recommended for development:
    - `dmidecode`
    - `lspci`
    - `clinfo`
    - `lsusb`
    - `tar`
    - `vainfo`
    - `grep`
    - `libva`
    - `libnuma`
    - (Debian distros) `dpkg`
    - (RPM distros) `rpm`
- Windows only: 
  - `pkg-config`
    - Install `pkgconfiglite` via Chocolatey: `choco install pkgconfiglite`
    - Chocolatey can be installed by following the [official instructions](https://chocolatey.org/install)

### Install Conan
```sh
pip install "conan>=2.0"
```

### Configure Conan profile (first time only)
```sh
conan profile detect --force
```

**Note: the `compiler.cppstd` field should be `20`**

For Linux, a reasonable default profile would be:
<details>
<summary>Example Linux profile</summary>

```sh
$ conan profile show
Host profile:
[settings]
arch=x86_64
build_type=Release
compiler=gcc
compiler.cppstd=20
compiler.libcxx=libstdc++11
compiler.version=13
os=Linux

Build profile:
[settings]
arch=x86_64
build_type=Release
compiler=gcc
compiler.cppstd=20
compiler.libcxx=libstdc++11
compiler.version=13
os=Linux
```

</details>

<details>
<summary>Example Windows profile</summary>

```sh
$ conan profile show
Host profile:
[settings]
arch=x86_64
build_type=Release
compiler=msvc
compiler.cppstd=20
compiler.runtime=dynamic
compiler.runtime_type=Release
compiler.version=194
os=Windows

Build profile:
[settings]
arch=x86_64
build_type=Release
compiler=msvc
compiler.cppstd=20
compiler.runtime=dynamic
compiler.runtime_type=Release
compiler.version=194
os=Windows
```

</details>

### Create Conan recipes (first time only)

When using Conan to manage build dependencies (recommended), the local recipes must be created once:
```sh
conan create recipes/level-zero
conan create recipes/metee
conan create recipes/igsc
```
These are cached by Conan and only need to be re-run when the version in a recipe file changes.

Alternatively, the following dependencies can be manually cloned, built, and installed so Conan can detect them as system packages (minimum versions must satisfy what the recipes require):

1. https://github.com/intel/igsc
2. https://github.com/intel/metee
3. https://github.com/oneapi-src/level-zero
4. https://github.com/CLIUtils/CLI11 (v2.6.2 or later)

### Build steps (using Conan)

#### Linux

```sh
# Install dependencies and configure build
conan install . --output-folder=.conan/ --build=missing -s build_type=Release

# Configure Meson build
meson setup builddir --native-file .conan/conan_meson_native.ini

# Build the project
meson compile -C builddir

# Install (optional)
meson install -C builddir
```

#### Windows

Run the following in a **Developer PowerShell for VS 2022** (or an `x64 Native Tools Command Prompt`):

```powershell
# Install dependencies and configure build
conan install . -s arch=x86_64 --output-folder=.conan/ --build=missing -s build_type=Release

# Configure Meson build
meson setup builddir --native-file .conan/conan_meson_native.ini

# Build the project
meson compile -C builddir

# Install (optional)
meson install -C builddir
```

### Debug Build (using Conan)

```sh
# Dependencies are built in Release; only xpum itself is built in Debug.
# -D werror=false can be used to disable warnings-as-errors for development builds
conan install . --output-folder=.conan/ --build=missing -s build_type=Release
meson setup builddir --native-file .conan/conan_meson_native.ini -D buildtype=debug -D werror=false
meson compile -C builddir
```