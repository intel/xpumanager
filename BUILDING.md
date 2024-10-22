# Build XPUM Installer

## Prepare environment and source tree
```sh
# only needed if running behind proxy
export http_proxy=...
export https_proxy=...

# prepare source
rm -fr /tmp/xpum_src
xpum_git_clone_options="--depth 1 -c http.sslVerify=false"
xpum_git_repo=https://github.com/intel/xpumanager.git
xpum_git_branch=master

git clone $xpum_git_clone_options \
    -b $xpum_git_branch $xpum_git_repo /tmp/xpum_src

git_commit=$(git -C /tmp/xpum_src rev-parse --short HEAD)

cd /tmp/xpum_src
mkdir -p .ccache
```

# Build .deb package for Ubuntu 20.04 / 22.04
```sh
# in /tmp/xpum_src

# Create builder
BASE_VERSION=20.04 # or 22.04
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--iidfile /tmp/xpum_builder_ubuntu_$BASE_VERSION.iid \
-f builder/Dockerfile.builder-ubuntu$BASE_VERSION .

# Build xpumanager package
rm -fr build
sudo docker run --rm \
    -v $PWD:$PWD \
    -u $UID \
    -e CCACHE_DIR=$PWD/.ccache \
    -e CCACHE_BASEDIR=$PWD \
    $(cat /tmp/xpum_builder_ubuntu_$BASE_VERSION.iid) $PWD/build.sh

## ==> $PWD/build/xpumanager*.deb generated

# Build xpu-smi package
rm -fr build
sudo docker run --rm \
    -v $PWD:$PWD \
    -u $UID \
    -e CCACHE_DIR=$PWD/.ccache \
    -e CCACHE_BASEDIR=$PWD \
    $(cat /tmp/xpum_builder_ubuntu_$BASE_VERSION.iid) $PWD/build.sh -DDAEMONLESS=ON

## ==> $PWD/build/xpu-smi*.deb generated
```

# Build .deb package for Debian 10 / 12

```sh
# in /tmp/xpum_src

# Create builder
BASE_VERSION=10 # or 12
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--iidfile /tmp/xpum_builder_debian_$BASE_VERSION.iid \
-f builder/Dockerfile.builder-debian$BASE_VERSION .

# Build xpumanager package
rm -fr build
sudo docker run --rm \
    -v $PWD:$PWD \
    -u $UID \
    -e CCACHE_DIR=$PWD/.ccache \
    -e CCACHE_BASEDIR=$PWD \
    $(cat /tmp/xpum_builder_debian_$BASE_VERSION.iid) $PWD/build.sh

## ==> $PWD/build/xpumanager*.deb generated

# Build xpu-smi package
rm -fr build
sudo docker run --rm \
    -v $PWD:$PWD \
    -u $UID \
    -e CCACHE_DIR=$PWD/.ccache \
    -e CCACHE_BASEDIR=$PWD \
    $(cat /tmp/xpum_builder_debian_$BASE_VERSION.iid) $PWD/build.sh -DDAEMONLESS=ON

## ==> $PWD/build/xpu-smi*.deb generated
```

# Build .rpm package for Redhat / CentOS 7

```sh
# in /tmp/xpum_src

# Create builder
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--iidfile /tmp/xpum_builder_centos7.iid \
-f builder/Dockerfile.builder-centos7 .

# Build xpumanager package
rm -fr build
sudo docker run --rm \
    -v $PWD:$PWD \
    -u $UID \
    -e CCACHE_DIR=$PWD/.ccache \
    -e CCACHE_BASEDIR=$PWD \
    $(cat /tmp/xpum_builder_centos7.iid) $PWD/build.sh

## ==> $PWD/build/xpumanager*.rpm generated
```

# Build .rpm package for Redhat / CentOS 8
```sh
# in /tmp/xpum_src

# Create builder
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--iidfile /tmp/xpum_builder_centos8.iid \
-f builder/Dockerfile.builder-centos8 .

# Build xpumanager package
rm -fr build
sudo docker run --rm \
    -v $PWD:$PWD \
    -u $UID \
    -e CCACHE_DIR=$PWD/.ccache \
    -e CCACHE_BASEDIR=$PWD \
    $(cat /tmp/xpum_builder_centos8.iid) $PWD/build.sh

## ==> $PWD/build/xpumanager*.rpm generated

# Build xpu-smi package
rm -fr build
sudo docker run --rm \
    -v $PWD:$PWD \
    -u $UID \
    -e CCACHE_DIR=$PWD/.ccache \
    -e CCACHE_BASEDIR=$PWD \
    $(cat /tmp/xpum_builder_centos8.iid) $PWD/build.sh -DDAEMONLESS=ON

## ==> $PWD/build/xpu-smi*.rpm generated.
```

# Build .rpm package for CentOS Stream 9
```sh
# in /tmp/xpum_src

# Create builder
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--iidfile /tmp/xpum_builder_centos_stream9.iid \
-f builder/Dockerfile.builder-centos-stream9 .

# Build xpumanager package
rm -fr build
sudo docker run --rm \
    -v $PWD:$PWD \
    -u $UID \
    -e CCACHE_DIR=$PWD/.ccache \
    -e CCACHE_BASEDIR=$PWD \
    $(cat /tmp/xpum_builder_centos_stream9.iid) $PWD/build.sh

## ==> $PWD/build/xpumanager*.rpm generated

# Build xpu-smi package
rm -fr build
sudo docker run --rm \
    -v $PWD:$PWD \
    -u $UID \
    -e CCACHE_DIR=$PWD/.ccache \
    -e CCACHE_BASEDIR=$PWD \
    $(cat /tmp/xpum_builder_centos_stream9.iid) $PWD/build.sh -DDAEMONLESS=ON

## ==> $PWD/build/xpu-smi*.rpm generated.
```

# Build .rpm package for SUSE
```sh
# in /tmp/xpum_src

# Create builder
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--iidfile /tmp/xpum_builder_sles.iid \
-f builder/Dockerfile.builder-sles .

# Build xpumanager package
rm -fr build
sudo docker run --rm \
    -v $PWD:$PWD \
    -u $UID \
    -e CCACHE_DIR=$PWD/.ccache \
    -e CCACHE_BASEDIR=$PWD \
    $(cat /tmp/xpum_builder_sles.iid) $PWD/build.sh

## ==> $PWD/build/xpumanager*.rpm generated

# Build xpu-smi package
rm -fr build
sudo docker run --rm \
    -v $PWD:$PWD \
    -u $UID \
    -e CCACHE_DIR=$PWD/.ccache \
    -e CCACHE_BASEDIR=$PWD \
    $(cat /tmp/xpum_builder_sles.iid) $PWD/build.sh -DDAEMONLESS=ON

## ==> $PWD/build/xpu-smi*.rpm generated.
```