#!/usr/bin/python3

from flask import Flask, json, jsonify
from flask import render_template
from flask import request

from DGMCore import DGMCore


app = Flask(__name__)

core = DGMCore()

@app.route('/rest/v1/version', methods=['GET'])
def get_version():
    code, message, data = core.getVersion()
    if code == 0:
        return jsonify(data)
    else:
        return message, 500
    

@app.route('/rest/v1/devices', methods=['GET'])
def get_devices():
    code, message, data = core.getDeviceList()
    if code == 0:
        return jsonify(data)
    else:
        return message, 500


@app.route('/rest/v1/devices/<int:deviceId>', methods=['GET'])
def get_device_properties(deviceId):
    try:
        code, message, data = core.getDeviceProperties(int(deviceId))
        if code == 0:
            return jsonify(data)
        error = dict(Status=code, Message=message)
        return jsonify(error), 400
    except Exception as e:
        print(e)
        return "Internal error", 500

@app.route('/rest/v1/groups', methods=['POST','GET'])
def groups():
    try:
        if request.method == 'POST':
            # create group
            req = request.get_json()
            groupName=req["GroupName"]
            code, message, data = core.createGroup(groupName)
            if code == 0:
                return jsonify(data)
            error = dict(Status=code, Message=message)
            return jsonify(error), 400
        elif request.method == 'GET':
            # get all group ids
            code, message, data = core.getAllGroupIds()
            if code == 0:
                return jsonify(data)
            error = dict(Status=code, Message=message)
            return jsonify(error), 500
    except Exception as e:
        print(e)
        return "Internal error", 500

@app.route('/rest/v1/groups/<int:groupId>', methods=['GET','POST','DELETE'])
def group_detail(groupId):
    if request.method == 'GET':
        # get group info
        code, message, data = core.getGroupInfo(groupId)
        if code == 0:
            return jsonify(data)
        error = dict(Status=code, Message=message)
        return jsonify(error), 400   
    elif request.method == 'DELETE':
        # destroy group
        code, message, data = core.destroyGroup(groupId)
        if code == 0:
            return "", 200
        error = dict(Status=code, Message=message)
        return jsonify(error), 400
    elif request.method == 'POST':
        req = request.get_json()

        # add device to group
        if "DeviceIdToAdd" in req:
            deviceIdToAdd = req["DeviceIdToAdd"]
            code, message, data = core.addDeviceToGroup(groupId,deviceIdToAdd)
            if code != 0:
                error = dict(Status=code, Message=message)
                return jsonify(error), 400
        
        # remove device from group
        if "DeviceIdToRemove" in req:
            deviceIdToRemove = req["DeviceIdToRemove"]
            code, message, data = core.removeDeviceFromGroup(groupId,deviceIdToRemove)
            if code != 0:
                error = dict(Status=code, Message=message)
                return jsonify(error), 400
        return jsonify(data), 200

@app.route('/rest/v1/globalSettings', methods=['GET','POST'])
def agent_setting():
    if request.method == 'GET':
        code, message, data = core.getAllAgentConfig()
        if code != 0:
            error = dict(Status=code, Message=message)
            return jsonify(error), 400
        return jsonify(data), 200
    elif request.method == 'POST':
        req = request.get_json()
        if "XPUM_AGENT_CONFIG_SAMPLE_INTERVAL" in req:
            value = req["XPUM_AGENT_CONFIG_SAMPLE_INTERVAL"]
            code, message, data = core.setAgentConfig("XPUM_AGENT_CONFIG_SAMPLE_INTERVAL", value)
            if code != 0:
                error = dict(Status=code, Message=message)
                return jsonify(error), 400
            return "", 200
        else:
            return dict(Status=1,Message="Illegal key"), 400

@app.route('/rest/v1/devices/<int:deviceId>/stats', methods=['GET'])
def get_statistics(deviceId):
    code, message, data = core.getStatistics(deviceId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)

@app.route('/rest/v1/groups/<int:groupId>/stats', methods=['GET'])
def get_group_statistics(groupId):
    code, message, data = core.getStatisticsByGroup(groupId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)

@app.route('/rest/v1/devices/<int:deviceId>/firmwareUpgrade', methods=['POST'])
def run_firmware_flash( deviceId ):
    firmwareType = request.form.get( 'type', type = int , default = 0 )
    filePath = request.form.get( 'file', type = str, default = '' )
    rc = core.runFirmwareFlash( deviceId, firmwareType, filePath )

    return jsonify( { 'result' : rc } )

@app.route('/rest/v1/devices/<int:deviceId>/firmware', methods=['GET'])
def get_firmware_flash_result( deviceId ):
    firmwareType = request.args.get( 'type', type = int, default = 0 )

    rc = core.getFirmwareFlashResult( deviceId, firmwareType )
    
    return jsonify( { 'result' : rc } )

if __name__ == '__main__':
  app.debug = True
#   app.run()
  app.run(port=30000)
