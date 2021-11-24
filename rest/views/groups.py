from flask import request, jsonify
import stub
from marshmallow import Schema, fields


class CreateGroupSchema(Schema):
    GroupName = fields.Str(
        metadata={"description": "The name for the group to be created"})


class GroupInfoSchema(Schema):
    group_name = fields.Str(metadata={"description": "The name of the group"})
    group_id = fields.Str(metadata={"description": "The id of the group"})
    device_id_list = fields.List(fields.Int(metadata={
                                 "description": "The id of devices belong to this group"}) )


def groups():
    """
    Create group / Get all group ids
    ---
    post:
        tags:
            - "Group Management"
        description: Create a new group
        consumes:
            - application/json
        parameters:
            - 
                name: group info
                in: body
                description: Information needed to create a group
                schema: CreateGroupSchema
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: GroupInfoSchema
            500:
                description: Error
    get:
        tags:
            - "Group Management"
        description: Get all group ids
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: 
                    type: array
                    items: 
                        type: integer
                        description: The id of groups
            500:
                description: Error
    """
    try:
        if request.method == 'POST':
            # create group
            req = request.get_json()
            if (req is None):
                return "requires group name.", 500

            groupName = req["GroupName"]
            code, message, data = stub.createGroup(groupName)
            if code == 0:
                return jsonify(data)
            error = dict(Status=code, Message=message)
            return jsonify(error), 400
        elif request.method == 'GET':
            # get all group ids
            code, message, data = stub.getAllGroups()
            if code == 0:
                return jsonify(data)
            error = dict(Status=code, Message=message)
            return jsonify(error), 500
    except Exception as e:
        #print(e)
        return "Internal error: " + e.description, 500


class ModifyGroupDeviceSchema(Schema):
    DeviceIdToAdd = fields.List(fields.Int(metadata={
                                 "description": "The id of devices add to this group"}))
    DeviceIdToRemove = fields.List(fields.Int(metadata={
                                 "description": "The id of devices remove from this group"}))

class ModifyGroupErrorSchema(Schema):
    device_id = fields.Int(metadata={
        "description": "The id of device failed to be added to or removed from the group"})
    error_msg = fields.Str(metadata={
        "description": "Error message"})


class ModifyGroupResponseSchema(Schema):
    fail_to_add = fields.Nested(ModifyGroupErrorSchema(many=True))
    fail_to_remove = fields.Nested(ModifyGroupErrorSchema(many=True))
    group_info = fields.Nested(GroupInfoSchema())

def group_detail(groupId):
    """
    Get group info / Modify group / Delete group
    ---
    get:
        tags:
            - "Group Management"
        description: Get information of a group
        parameters:
            - 
                name: groupId
                in: path
                description: Group id
                type: integer
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: GroupInfoSchema
            400:
                description: Error
    post:
        tags:
            - "Group Management"
        description: Modify a group
        parameters:
            - 
                name: modify info
                in: body
                description: Device ids to add to or remove from a group
                schema: ModifyGroupDeviceSchema
            -
                name: groupId
                in: path
                description: Group id
                type: integer
        responses:
            200:
                description: OK
                schema: ModifyGroupResponseSchema
            400:
                description: Error
    delete:
        tags:
            - "Group Management"
        description: Delete a group
        responses:
            200:
                description: OK
            400:
                description: Error
    """
    try:
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

            data = dict()

            # add device to group
            if "DeviceIdToAdd" in req:
                deviceIdToAdd = req["DeviceIdToAdd"]
                if len(deviceIdToAdd) == 0:
                    return "requires device id.", 500
                fail_to_add = stub.addDeviceToGroup(groupId, deviceIdToAdd)
                if fail_to_add:
                    data["fail_to_add"] = fail_to_add

            # remove device from group
            if "DeviceIdToRemove" in req:
                deviceIdToRemove = req["DeviceIdToRemove"]
                if len(deviceIdToRemove) == 0:
                    return "requires device id.", 500
                fail_to_remove = stub.removeDeviceFromGroup(
                    groupId, deviceIdToRemove)
                if fail_to_remove:
                    data["fail_to_remove"] = fail_to_remove

            # get group info
            code, message, group_info = stub.getGroupInfo(groupId)

            data["group_info"] = group_info

            return jsonify(data), 200
    except Exception as e:
        #print(e)
        return "Internal error: " + e.description, 500
