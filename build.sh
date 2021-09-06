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

if [ ! -f /usr/local/hwloc/lib/libhwloc.a ]; then
    echo "build hwloc"
    cd core/libs/hwloc
    ./autogen.sh --with-pic
    ./configure  --prefix=/usr/local/hwloc --enable-static --disable-shared LDFLAGS="--static" CFLAGS="-fPIC"
    make -j
    make install
fi

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
