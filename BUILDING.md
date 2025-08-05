# Building XPUM

This document describes how to build XPUM using either the new Conan-based build system or the traditional manual dependency management.

## Recommended: Build with Conan (Automated Dependencies)

### Prerequisites
- Python 3.8+
- Conan 2.0+
- GCC/Clang with C++20 support
- Meson and Ninja (will be installed automatically by Conan)

### Install Conan
```sh
pip install conan>=2.0
```

### Configure Conan profile (first time only)
```sh
conan profile detect --force
```

**Note: the `compiler.cppstd` field should be `20`**

### Build Steps
```sh
# Clone the repository with submodules
git clone --recurse-submodules <repository-url>
cd xpum

# Install dependencies and configure build
conan install . --build=missing -s build_type=Release

# Configure Meson build
meson setup builddir --native-file conan_meson_native.ini

# Build the project
meson compile -C builddir

# Install (optional)
meson install -C builddir
```

### Debug Build
```sh
# For debug builds
conan install . --build=missing -s build_type=Debug
meson setup builddir --native-file conan_meson_native.ini -Dbuildtype=debug
meson compile -C builddir
```

### Build Options
You can customize the build with Conan options:
```sh
# Build with tests enabled
conan install . -o with_tests=True --build=missing

# Build shared libraries
conan install . -o shared=True --build=missing
```

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