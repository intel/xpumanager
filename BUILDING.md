# Building XPUM

This document describes how to build XPUM using either the new Conan-based build system or the traditional manual dependency management.

## Recommended: Build with Conan (Automated Dependencies)

### Prerequisites
- Python 3.8+
- Conan 2.0+
- C++ compiler with C++20 support
- Meson
- (Windows only) `pkg-config`
   - It is recommended to install `pkgconfiglite` via `choco`
   - `choco` can be installed via `dt`

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

### Build Steps (first build)
Optionally, when using Conan to manage our build dependencies (recommended), the recipes must be created via:
```sh
conan create recipes/level-zero --name=level-zero --version=1.23.1
conan create recipes/metee --name=metee --version=6.0.0
conan create recipes/igsc --name=igsc --version=0.9.6
```
Don't worry, they will be cached and do not need to be rebuilt outside of a version bump.

Otherwise, the following dependencies can be manually cloned and built to be identified as system packages (Note: the version numbers should match what the Conan recipes have listed):
1. https://github.com/intel/igsc
2. https://github.com/oneapi-src/level-zero

### Build steps (using Conan)
```sh
# (If using Conan) Install dependencies and configure build
conan install . --output-folder=.conan/ --build=missing -s build_type=Release

# Configure Meson build
meson setup builddir --native-file .conan/conan_meson_native.ini

# Build the project
meson compile -C builddir

# Install (optional)
meson install -C builddir
```

### Debug Build (using Conan)
```sh
# For debug builds (note: this will build our dependencies in release but xpum in debug)
conan install . --output-folder=.conan/ --build=missing -s build_type=Release
meson setup builddir --native-file .conan/conan_meson_native.ini -D buildtype=debug
meson compile -C builddir
```

### Build steps (using system packages)
Note: the Intel IGSC and level-zero packages must be built and installed on your system:
```sh
meson setup builddir \
      -D use_system_levelzero=true \
      -D use_system_igsc=true
```

### Build options
The `meson_options.txt` file provides all custom options, including potentially useful toggles such as `-D dev=true` to temporarily disable warnings as errors.

---

## Alternative: Manual Build (Legacy)

### Build for Ubuntu 24.04

#### Build Level Zero Loader dependency
```sh
1) git clone https://github.com/oneapi-src/level-zero.git
2) Build level-zero loader from the above cloned repository as per its Readme
3) export LEVELZERO=`path to the level-zero folder`
```

# Build igsc dependency
```sh
1) git clone https://github.com/intel/igsc.git
2) Build igsc lib from the above cloned repository as per its Readme
3) export IGSC=`path to the igsc folder`
```

#### Build and run xpum
```sh
1) From the main folder, type make -j`# of processors present on the system`
OR
2) To build debug, type make debug -j`# of processors present on the system`
3) export LD_LIBRARY_PATH=`path to the level-zero folder/build/lib/`:`path to the igsc folder/build/lib`:`path to xpum/hal/core`
4) Before you can run this executable on a Linux target, you will need to install Level Zero and its dependencies. Please follow the instructions located at: https://github.com/intel/compute-runtime/releases/
5) cd ial/cli
6) ./xpu-smi <commands>
```

### Build for Windows

#### Build Level Zero Loader dependency
```sh
1) git clone https://github.com/oneapi-src/level-zero.git
2) Build level-zero loader from the above cloned repository as per its Readme
3) Create an environment variable LEVELZERO through Windows pointing to the location of the above level-zero loader path
```

# Build igsc dependency
```sh
1) git clone https://github.com/intel/igsc.git
2) Build igsc lib from the above cloned repository as per its Readme
3) Create an environment variable IGSC through Windows pointing to the location of the above igsc path
```

#### Build and run xpum
```sh
1) Build for either release or debug mode in Visual Studio 2022.
2) Ensure that igsc.dll, ze_loader.dll and libxpum.dll in your path.
3) Ensure that you have the latest Windows graphics driver installed. Level Zero and its dependencies will also be installed.
4) cd ial/cli
5) ./xpu-smi <commands>
```