# Build for Ubuntu 24.04

# Build Level Zero Loader dependency
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


# Build and run xpum
```sh
1) From the main folder, type make -j`# of processors present on the system`
OR
2) To build debug, type make debug -j`# of processors present on the system`
3) export LD_LIBRARY_PATH=`path to the level-zero folder/build/lib/`:`path to the igsc folder/build/lib`:`path to xpum/hal/core`
4) Before you can run this executable on a Linux target, you will need to install Level Zero and its dependencies. Please follow the instructions located at: https://github.com/intel/compute-runtime/releases/
5) cd ial/cli
6) ./xpu-smi <commands>
```


# Build for Windows

# Build Level Zero Loader dependency
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

# Build and run xpum
```sh
1) Build for either release or debug mode in Visual Studio 2022.
2) Ensure that igsc.dll, ze_loader.dll and libxpum.dll in your path.
3) Ensure that you have the latest Windows graphics driver installed. Level Zero and its dependencies will also be installed.
4) cd ial/cli
5) ./xpu-smi <commands>
```