#!/bin/bash

WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`

rm -rf pci.ids.upgrade
curl --proxy http://child-prc.intel.com:912 -o pci.ids.upgrade "https://pci-ids.ucw.cz/v2.2/pci.ids" -S
if [ $? -eq 0 ]; then    
    cp -r pci.ids.upgrade ./core/config/pci.ids
else
    echo "upgrade pci.ids failed."
    exit 1
fi

echo "build hwloc"
cd core/libs/hwloc
./configure --enable-static --disable-shared LDFLAGS="--static"
make -j

cp "hwloc/.libs/libhwloc.a" ${WORK_DIR}/core/libs/

echo "build distribution package"
cd ${WORK_DIR}
rm -rf build
mkdir build
cd build
cmake ..  $@
make -j
cpack    

if [ -f ~/password.sys_dcm ]; then
    PackageName=$(cat package_file_name)
    CSUser="ccr\\sys_dcm"
    CSPwd=$(cat ~/password.sys_dcm)
    echo "SignFile:${PackageName}" 
    "${WORK_DIR}"/tools/signfile/SignFile -vv -u "${CSUser}" -p "${CSPwd}" -s cl -cf ${PackageName}.sig ${PackageName}
fi
