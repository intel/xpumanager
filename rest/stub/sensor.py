#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file sensor.py
#

from google.protobuf import empty_pb2
from .grpc_stub import stub


def getAMCSensorReading():
    resp = stub.getAMCSensorReading(empty_pb2.Empty())
    if len(resp.errorMsg) != 0:
        return resp.errorNo, resp.errorMsg, None
    data = dict()
    data_list = []
    for sensor in resp.dataList:
        obj = dict()
        obj["amc_index"] = sensor.deviceIdx
        obj["value"] = sensor.value
        obj["sensor_name"] = sensor.sensorName
        obj["sensor_high"] = sensor.sensorHigh
        obj["sensor_low"] = sensor.sensorLow
        obj["sensor_unit"] = sensor.sensorUnit
        data_list.append(obj)
    data["sensor_reading"] = data_list
    return 0, "", data
