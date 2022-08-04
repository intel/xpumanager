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
```

# Build .deb package
```sh
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--build-arg XPUM_GIT_COMMIT=$git_commit \
--iidfile /tmp/xpum_builder_ubuntu.iid \
-f /tmp/xpum_src/builder/Dockerfile.builder-ubuntu /tmp/xpum_src

# Retrieve package
containerid=$(sudo docker create $(cat /tmp/xpum_builder_ubuntu.iid))
sudo docker cp $containerid:/artifacts/. .
sudo docker rm -v $containerid
```

# Build .rpm package for Redhat / CentOS 7 
```sh
# in /tmp/xpum_src
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--build-arg XPUM_GIT_COMMIT=$git_commit \
--iidfile /tmp/xpum_builder_centos7.iid \
-f /tmp/xpum_src/builder/Dockerfile.builder-centos7 /tmp/xpum_src

# Retrieve package
containerid=$(sudo docker create $(cat /tmp/xpum_builder_centos7.iid))
sudo docker cp $containerid:/artifacts/. .
sudo docker rm -v $containerid
```

# Build .rpm package for Redhat / CentOS 8
```sh
# in /tmp/xpum_src
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--build-arg XPUM_GIT_COMMIT=$git_commit \
--iidfile /tmp/xpum_builder_centos8.iid \
-f /tmp/xpum_src/builder/Dockerfile.builder-centos8 /tmp/xpum_src

# Retrieve package
containerid=$(sudo docker create $(cat /tmp/xpum_builder_centos8.iid))
sudo docker cp $containerid:/artifacts/. .
sudo docker rm -v $containerid
```

# Build .rpm package for SUSE
```sh
# in /tmp/xpum_src
sudo docker build \
--build-arg http_proxy=$http_proxy \
--build-arg https_proxy=$https_proxy \
--build-arg XPUM_GIT_COMMIT=$git_commit \
--iidfile /tmp/xpum_builder_sles.iid \
-f /tmp/xpum_src/builder/Dockerfile.builder-sles /tmp/xpum_src

# Retrieve package
containerid=$(sudo docker create $(cat /tmp/xpum_builder_sles.iid))
sudo docker cp $containerid:/artifacts/. .
sudo docker rm -v $containerid
```
