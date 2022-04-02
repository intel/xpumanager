#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file versions.py
#

from google.protobuf import empty_pb2
from .grpc_stub import stub


def getVersion():
    resp = stub.getVersion(empty_pb2.Empty())
    if len(resp.errorMsg) != 0:
        print("error msg " + resp.errorMsg)
        return 1, "Fail to get version info", None
    data = dict()
    for version in resp.versions:
        # print( version.versionString )
        versionStr = version.versionString
        versionType = version.version.value
        if versionType == 0:
            data['xpum_version'] = versionStr
        elif versionType == 1:
            data['xpum_version_git'] = versionStr
        elif versionType == 2:
            data['level_zero_version'] = versionStr
    return 0, "OK", data
