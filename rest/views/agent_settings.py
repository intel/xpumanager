from flask import request, jsonify
import stub


def agent_setting():
    if request.method == 'GET':
        code, message, data = stub.getAllAgentConfig()
        if code != 0:
            error = dict(Status=code, Message=message)
            return jsonify(error), 400
        return jsonify(data), 200
    elif request.method == 'POST':
        req = request.get_json()
        if "XPUM_AGENT_CONFIG_SAMPLE_INTERVAL" in req:
            value = req["XPUM_AGENT_CONFIG_SAMPLE_INTERVAL"]
            code, message, data = stub.setAgentConfig(
                "XPUM_AGENT_CONFIG_SAMPLE_INTERVAL", value)
            if code != 0:
                error = dict(Status=code, Message=message)
                return jsonify(error), 400
            return "", 200
        else:
            return dict(Status=1, Message="Illegal key"), 400
