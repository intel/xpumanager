import stub
from flask import jsonify


def get_version():
    code, message, data = stub.getVersion()
    if code == 0:
        return jsonify(data)
    else:
        return message, 500
