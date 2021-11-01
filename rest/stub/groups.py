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
