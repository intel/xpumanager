#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file groups.py
#

from .grpc_stub import stub, exit_on_disconnect
import core_pb2
from google.protobuf import empty_pb2
import xpum_logger as logger


def createGroup(groupName):
    resp = stub.groupCreate(core_pb2.GroupName(name=groupName))
    if len(resp.errorMsg) != 0:
        logger.audit("Group", "Failed", "Fail to create group {}", groupName)
        return 1, resp.errorMsg, None
    logger.audit("Group", "Succeed",
                 "Succeed to create group {} ：{}", resp.id, groupName)
    data = dict()
    data["group_name"] = groupName
    data["group_id"] = resp.id
    data["device_id_list"] = []
    return 0, "OK", data


@exit_on_disconnect
def getAllGroups():
    resp = stub.getAllGroups(empty_pb2.Empty())
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = []
    for d in resp.groupList:
        g = dict()
        g["group_id"] = d.id
        g["group_name"] = d.groupName
        g["device_id_list"] = [i.id for i in d.deviceList]
        data.append(g)
    return 0, "OK", data


def getGroupInfo(groupId):
    resp = stub.groupGetInfo(core_pb2.GroupId(id=groupId))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = dict()
    data["group_name"] = resp.groupName
    data["group_id"] = groupId
    data["device_id_list"] = [i.id for i in resp.deviceList]
    return 0, "OK", data


def destroyGroup(groupId):
    resp = stub.groupDestory(core_pb2.GroupId(id=groupId))
    if len(resp.errorMsg) != 0:
        logger.audit("Group", "Failed", "Fail to delete group {}", groupId)
        return 1, resp.errorMsg, None
    logger.audit("Group", "Succeed", "Succeed to delete group {}", groupId)
    return 0, "OK", None


def addDeviceToGroup(groupId, deviceIds):
    fail_to_add = []
    for deviceId in deviceIds:
        resp = stub.groupAddDevice(core_pb2.GroupAddRemoveDevice(
            groupId=groupId, deviceId=deviceId))
        if len(resp.errorMsg) != 0:
            logger.audit("Group", "Failed",
                         "Fail to add device {} to group {}", deviceId, groupId)
            fail_to_add.append(
                dict(device_id=deviceId, error_msg=resp.errorMsg))
        else:
            logger.audit(
                "Group", "Succeed", "Succeed to add device {} to group {}", deviceId, groupId)
    return fail_to_add


def removeDeviceFromGroup(groupId, deviceIds):
    fail_to_remove = []
    for deviceId in deviceIds:
        resp = stub.groupRemoveDevice(core_pb2.GroupAddRemoveDevice(
            groupId=groupId, deviceId=deviceId))
        if len(resp.errorMsg) != 0:
            logger.audit(
                "Group", "Failed", "Fail to remove device {} from group {}", deviceId, groupId)
            fail_to_remove.append(
                dict(device_id=deviceId, error_msg=resp.errorMsg))
        else:
            logger.audit(
                "Group", "Succeed", "Succeed to remove device {} from group {}", deviceId, groupId)
    return fail_to_remove
