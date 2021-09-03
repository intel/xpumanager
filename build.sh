#!/bin/bash

WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`
HWLOC=hwloc-2.4.1

rm -rf pci.ids.upgrade
curl --proxy http://child-prc.intel.com:912 -o pci.ids.upgrade "https://pci-ids.ucw.cz/v2.2/pci.ids" -S
if [ $? -eq 0 ]; then    
    cp -r pci.ids.upgrade ./core/config/pci.ids
else
    echo "upgrade pci.ids failed."
    exit 1
fi

echo "build hwloc"
cd core/libs
tar -zxvf ${HWLOC}.tar.gz
cd ${HWLOC}
./configure --enable-static --disable-shared LDFLAGS="--static"
make -j

cp "hwloc/.libs/libhwloc.a" ${WORK_DIR}/core/libs/

cd ${WORK_DIR}
if [ -f "/etc/debian_version" ]; then
    rm -rf build-deb
    mkdir build-deb
    cd build-deb
    cmake ..  $@
    make -j
    cpack    
elif [ -f "/etc/redhat-release" ] || [ -f "/etc/SUSE-release" ]; then
    rm -rf build-rpm
    mkdir build-rpm
    cd build-rpm
    cmake ..  $@
    make -j
    cpack
fi

if [ -f ~/password.sys_dcm ]; then
    PackageName=$(cat package_file_name)
    CSUser="ccr\\sys_dcm"
    CSPwd=$(cat ~/password.sys_dcm)
    echo "SignFile:${PackageName}" 
    "${WORK_DIR}"/tools/signfile/SignFile -vv -u "${CSUser}" -p "${CSPwd}" -s cl -cf ${PackageName}.sig ${PackageName}
fi
