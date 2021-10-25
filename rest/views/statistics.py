from flask import jsonify
import stub

def get_statistics(deviceId):
    """
    Get statistics by device
    """
    code, message, data = stub.getStatistics(deviceId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def get_group_statistics(groupId):
    code, message, data = stub.getStatisticsByGroup(groupId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)