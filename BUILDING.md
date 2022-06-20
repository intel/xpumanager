# Build XPUM Installer

## Build .deb installer on Ubuntu 20.04

```sh
# only needed if running behind proxy
export http_proxy=http://child-prc.intel.com:913
export https_proxy=http://child-prc.intel.com:913

# If running in Intel network, use internal Ubuntu mirror to speed up
echo deb http://linux-ftp.sh.intel.com/pub/mirrors/ubuntu/ focal main universe > /etc/apt/sources.list
echo deb http://linux-ftp.sh.intel.com/pub/mirrors/ubuntu/ focal-updates main universe >> /etc/apt/sources.list
export no_proxy=.intel.com

sudo apt-get update

# Install prerequisites
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y autoconf automake build-essential cmake doxygen \
  dpkg-dev git gcc g++ libssl-dev libtool liblua5.2-0 pkg-config python3 python3-dev python3-pip wget

# Remove pciaccess to avoid runtime dependency to its shared library
sudo apt-get remove -y libpciaccess-dev libpciaccess0

# Install python packages
pip3 --proxy=$http_proxy install grpcio-tools mistune==0.8.4 apispec apispec_webframeworks Sphinx \
    sphinx_rtd_theme sphinxcontrib-openapi apispec-webframeworks myst-parser marshmallow \
    prometheus-client flask flask_httpauth

# Install level-zero
mkdir work && cd work
wget -q --no-check-certificate \
https://github.com/oneapi-src/level-zero/releases/download/v1.8.1/level-zero-devel_1.8.1+u18.04_amd64.deb
wget -q --no-check-certificate \
https://github.com/oneapi-src/level-zero/releases/download/v1.8.1/level-zero_1.8.1+u18.04_amd64.deb
sudo dpkg -i *.deb

# Get XPUM source
xpum_git_clone_options="--depth 1 -c http.sslVerify=false"
xpum_git_repo=https://sh1svsts006.ccr.corp.intel.com/tfs/DcmCollection/XPUManagerGit/_git/XPUManagerGit
xpum_git_branch=master
git clone $xpum_git_clone_options -b $xpum_git_branch $xpum_git_repo /tmp/xpum_src

# Build and install gRPC first
git clone --depth 1 -b v1.45.0 https://github.com/grpc/grpc /tmp/grpc_build
cd /tmp/grpc_build
git submodule update --init
cp /tmp/xpum_src/builder/build_grpc.sh .
./build_grpc.sh

# Build XPUM
cd /tmp/xpum_src
./build.sh -DGIT_COMMIT=$(git rev-parse --short HEAD) -DBUILD_DOC=ON
```

## Build .rpm installer on Red Hat Enterprise Linux / CentOS 8.4

```sh
# only needed on CentOS
sudo sed -i 's/mirrorlist/#mirrorlist/g' /etc/yum.repos.d/CentOS-*
sudo sed -i 's|#baseurl=http://mirror.centos.org|baseurl=http://vault.centos.org|g' /etc/yum.repos.d/CentOS-*
# end CentOS

# only needed if running behind proxy
export http_proxy=http://child-prc.intel.com:913
export https_proxy=http://child-prc.intel.com:913
# end proxy

sudo dnf update -y
sudo dnf clean all
sudo dnf install -y cmake dnf-plugins-core gcc gcc-c++ git libtool make openssl-devel pkg-config \
    python3 python3-devel python3-pip rpm-build wget

# only needed on CentOS
sudo dnf config-manager --set-enabled powertools
# end CentOS

# only needed on RHEL
sudo dnf config-manager --set-enabled CodeReadyBuilder
# end RHEL

sudo dnf install -y doxygen glibc-static lua-devel

# Remove pciaccess to avoid runtime dependency to its shared library
sudo dnf remove -y libpciaccess libpciaccess-devel

# Install python packages
pip3 --proxy=$http_proxy install grpcio-tools mistune==0.8.4 apispec apispec_webframeworks Sphinx \
    sphinx_rtd_theme sphinxcontrib-openapi apispec-webframeworks myst-parser marshmallow \
    prometheus-client flask flask_httpauth

# Build and install levelzero from its source
wget -qc --no-check-certificate https://github.com/oneapi-src/level-zero/archive/refs/tags/v1.8.1.tar.gz -O - | tar -xz
cd level-zero-1.8.1
mkdir build && cd build
cmake ..
make -j$(nproc) install

# Get XPUM source
xpum_git_clone_options="--depth 1 -c http.sslVerify=false"
xpum_git_repo=https://sh1svsts006.ccr.corp.intel.com/tfs/DcmCollection/XPUManagerGit/_git/XPUManagerGit
xpum_git_branch=master
git clone $xpum_git_clone_options -b $xpum_git_branch $xpum_git_repo /tmp/xpum_src

# Build and install gRPC first
git clone --depth 1 -b v1.45.0 https://github.com/grpc/grpc /tmp/grpc_build
cd /tmp/grpc_build
git submodule update --init
cp /tmp/xpum_src/builder/build_grpc.sh .
./build_grpc.sh

# Build XPUM
cd /tmp/xpum_src
./build.sh -DGIT_COMMIT=$(git rev-parse --short HEAD) -DBUILD_DOC=ON
```

## Build .deb and .rpm installers using Container
```sh
# only needed if running behind proxy
export http_proxy=http://child-prc.intel.com:913
export https_proxy=http://child-prc.intel.com:913

rm -fr /tmp/xpum_src
xpum_git_clone_options="--depth 1 -c http.sslVerify=false"
xpum_git_repo=https://sh1svsts006.ccr.corp.intel.com/tfs/DcmCollection/XPUManagerGit/_git/XPUManagerGit
xpum_git_branch=master
git clone $xpum_git_clone_options -b $xpum_git_branch $xpum_git_repo /tmp/xpum_src

cd /tmp/xpum_src

git_commit=$(git rev-parse --short HEAD)

# Build .deb installer
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--build-arg XPUM_GIT_COMMIT=$git_commit \
--iidfile /tmp/xpum_builder_ubuntu.iid \
-f ./builder/Dockerfile.builder-ubuntu .

containerid=$(sudo docker create $(cat /tmp/xpum_builder_ubuntu.iid))
sudo docker cp $containerid:/artifacts/. .
sudo docker rm -v $containerid

# Build .rpm installer
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--build-arg XPUM_GIT_COMMIT=$git_commit \
--iidfile /tmp/xpum_builder_centos.iid \
-f ./builder/Dockerfile.builder-centos .

containerid=$(sudo docker create $(cat /tmp/xpum_builder_centos.iid))
sudo docker cp $containerid:/artifacts/. .
sudo docker rm -v $containerid
```