from .grpc_stub import stub
import core_pb2
from google.protobuf import empty_pb2


def createGroup(groupName):
    resp = stub.groupCreate(core_pb2.GroupName(name=groupName))
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = dict()
    data["group_name"] = groupName
    data["group_id"] = resp.id
    data["device_id_list"] = []
    return 0, "OK", data


def getAllGroupIds():
    resp = stub.getAllGroupIds(empty_pb2.Empty())
    if len(resp.errorMsg) != 0:
        return 1, resp.errorMsg, None
    data = [groupId.id for groupId in resp.groupList]
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
        return 1, resp.errorMsg, None
    return 0, "OK", None


def addDeviceToGroup(groupId, deviceIds):
    for deviceId in deviceIds:
        resp = stub.groupAddDevice(core_pb2.GroupAddRemoveDevice(
            groupId=groupId, deviceId=deviceId))
        if len(resp.errorMsg) != 0:
            return 1, resp.errorMsg, None
    data = dict()
    data["group_name"] = resp.groupName
    data["group_id"] = groupId
    data["device_id_list"] = [i.id for i in resp.deviceList]
    return 0, "OK", data


def removeDeviceFromGroup(groupId, deviceIds):
    for deviceId in deviceIds:
        resp = stub.groupRemoveDevice(core_pb2.GroupAddRemoveDevice(
            groupId=groupId, deviceId=deviceId))
        if len(resp.errorMsg) != 0:
            return 1, resp.errorMsg, None
    data = dict()
    data["group_name"] = resp.groupName
    data["group_id"] = groupId
    data["device_id_list"] = [i.id for i in resp.deviceList]
    return 0, "OK", data
