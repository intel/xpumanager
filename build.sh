#!/usr/bin/env bash
#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file build.sh
#


WORK=`dirname "$0"`
WORK_DIR=`cd ${WORK} && pwd`

# if [ $(date -r ./core/resources/config/pci.ids +"%Y-%m-%d") == $(date +"%Y-%m-%d") ]; then
#     echo "pci.ids is up to date."
# else
#     rm -rf pci.ids.upgrade
#     curl --proxy http://child-prc.intel.com:912 -o pci.ids.upgrade "https://pci-ids.ucw.cz/v2.2/pci.ids" -S
#     if [ $? -eq 0 ]; then    
#         cp -r pci.ids.upgrade ./core/resources/config/pci.ids
#     else
#         echo "upgrade pci.ids failed."
#         exit 1
#     fi
# fi

echo "build distribution package"
cd ${WORK_DIR}
if [ -d build ]; then
    for f in $( ls -a build )
    do
        if [ x"$f" != x"." ] && [ x"$f" != x".." ] \
            && [ x"$f" != x"hwloc" ] \
            && [ x"$f" != x"third_party" ] \
            && [ x"$f" != x"lib" ]; then
            rm -rf build/$f
        fi
    done
    echo "build folder exist."
else
    mkdir build
fi
cd build
ccache_opts=
if command -v ccache &> /dev/null
then
    ccache_opts="-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
fi
cmake .. $ccache_opts $@
make -j4

echo "---------Create installation package-----------"
cpack   

if [ -f ~/password.sys_dcm ]; then
    PackageName=$(cat package_file_name)
    CSUser="ccr\\sys_dcm"
    CSPwd=$(cat ~/password.sys_dcm)
    echo "SignFile:${PackageName}" 
    "${WORK_DIR}"/install/tools/signfile/SignFile -vv -u "${CSUser}" -p "${CSPwd}" ${PackageName}
fi
