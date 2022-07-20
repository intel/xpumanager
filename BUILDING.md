# Build XPUM Installer

## Build .deb and .rpm packages using container
```sh
# only needed if running behind proxy
export http_proxy=...
export https_proxy=...

# prepare source
rm -fr /tmp/xpum_src
xpum_git_clone_options="--depth 1 -c http.sslVerify=false"
xpum_git_repo=https://github.com/intel/xpumanager.git
xpum_git_branch=master
git clone $xpum_git_clone_options -b $xpum_git_branch $xpum_git_repo /tmp/xpum_src

cd /tmp/xpum_src

git_commit=$(git rev-parse --short HEAD)

# Build .deb package
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--build-arg XPUM_GIT_COMMIT=$git_commit \
--iidfile /tmp/xpum_builder_ubuntu.iid \
-f ./builder/Dockerfile.builder-ubuntu .
# Retrieve .deb package
containerid=$(sudo docker create $(cat /tmp/xpum_builder_ubuntu.iid))
sudo docker cp $containerid:/artifacts/. .
sudo docker rm -v $containerid

# Build Redhat .rpm package
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--build-arg XPUM_GIT_COMMIT=$git_commit \
--iidfile /tmp/xpum_builder_centos.iid \
-f ./builder/Dockerfile.builder-centos .
# Retrieve Redhat .rpm package
containerid=$(sudo docker create $(cat /tmp/xpum_builder_centos.iid))
sudo docker cp $containerid:/artifacts/. .
sudo docker rm -v $containerid

# Build SUSE .rpm installer
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--build-arg XPUM_GIT_COMMIT=$git_commit \
--iidfile /tmp/xpum_builder_sles.iid \
-f ./builder/Dockerfile.builder-sles .
# Retrieve SUSE .rpm package
containerid=$(sudo docker create $(cat /tmp/xpum_builder_sles.iid))
sudo docker cp $containerid:/artifacts/. .
sudo docker rm -v $containerid
```
