from flask import request, jsonify
import stub
from marshmallow import Schema, fields

class CreateGroupSchema(Schema):
    GroupName = fields.Str()


class GroupInfoSchema(Schema):
    pass


def groups():
    try:
        if request.method == 'POST':
            # create group
            req = request.get_json()
            groupName = req["GroupName"]
            code, message, data = stub.createGroup(groupName)
            if code == 0:
                return jsonify(data)
            error = dict(Status=code, Message=message)
            return jsonify(error), 400
        elif request.method == 'GET':
            # get all group ids
            code, message, data = stub.getAllGroupIds()
            if code == 0:
                return jsonify(data)
            error = dict(Status=code, Message=message)
            return jsonify(error), 500
    except Exception as e:
        print(e)
        return "Internal error", 500


def group_detail(groupId):
    if request.method == 'GET':
        # get group info
        code, message, data = stub.getGroupInfo(groupId)
        if code == 0:
            return jsonify(data)
        error = dict(Status=code, Message=message)
        return jsonify(error), 400
    elif request.method == 'DELETE':
        # destroy group
        code, message, data = stub.destroyGroup(groupId)
        if code == 0:
            return "", 200
        error = dict(Status=code, Message=message)
        return jsonify(error), 400
    elif request.method == 'POST':
        req = request.get_json()

        # add device to group
        if "DeviceIdToAdd" in req:
            deviceIdToAdd = req["DeviceIdToAdd"]
            code, message, data = stub.addDeviceToGroup(groupId, deviceIdToAdd)
            if code != 0:
                error = dict(Status=code, Message=message)
                return jsonify(error), 400

        # remove device from group
        if "DeviceIdToRemove" in req:
            deviceIdToRemove = req["DeviceIdToRemove"]
            code, message, data = stub.removeDeviceFromGroup(
                groupId, deviceIdToRemove)
            if code != 0:
                error = dict(Status=code, Message=message)
                return jsonify(error), 400
        return jsonify(data), 200
