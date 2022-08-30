# Build XPUM Installer
These are building instructions assuming you are using docker. If docker is not used, please refer to builder/Dockerfile.builder-* for how to set up the building environment.
## Prepare environment and source tree
```sh
# only needed if running behind proxy
export http_proxy=...
export https_proxy=...

# prepare source
rm -fr /tmp/xpum_src
xpum_git_clone_options="--depth 1 -c http.sslVerify=false"
xpum_git_repo=https://github.com/intel/xpumanager.git
xpum_git_branch=xpu-smi

git clone $xpum_git_clone_options \
    -b $xpum_git_branch $xpum_git_repo /tmp/xpum_src

git_commit=$(git -C /tmp/xpum_src rev-parse --short HEAD)
```

# Build .deb package
```sh
cd /tmp/xpum_src
rm -fr build
mkdir -p .ccache
docker build \
    --build-arg UID=$(id -u) \
    --build-arg GID=$(id -g) \
    --build-arg http_proxy=$http_proxy \
    --build-arg https_proxy=$https_proxy \
    --iidfile /tmp/xpum_builder_ubuntu.iid \
    -f builder/Dockerfile.builder-ubuntu .
docker run \
    --rm \
    -v $PWD:$PWD \
    -w $PWD \
    -e CCACHE_DIR=$PWD/.ccache \
    -e CCACHE_BASEDIR=$PWD \
    $(cat /tmp/xpum_builder_ubuntu.iid) \
    ./build.sh \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

# /tmp/xpum_src/build/xpuxxxx.deb created
```

# Build .rpm package for Redhat / CentOS 8
```sh
cd /tmp/xpum_src
rm -fr build
mkdir -p .ccache
docker build \
    --build-arg UID=$(id -u) \
    --build-arg GID=$(id -g) \
    --build-arg http_proxy=$http_proxy \
    --build-arg https_proxy=$https_proxy \
    --iidfile /tmp/xpum_builder_centos.iid \
    -f builder/Dockerfile.builder-centos .
docker run \
    --rm \
    -v $PWD:$PWD \
    -w $PWD \
    -e CCACHE_DIR=$PWD/.ccache \
    -e CCACHE_BASEDIR=$PWD \
    $(cat /tmp/xpum_builder_centos.iid) \
    ./build.sh \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
    
# /tmp/xpum_src/build/xpuxxxx.rpm created
```
