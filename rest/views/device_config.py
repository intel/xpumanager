#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file device_config.py
#

from flask import request, jsonify
import stub
from marshmallow import Schema, fields


class StandbySchema(Schema):
    tile_id = fields.Integer(
        metadata={"description": "The tile id"})
    standby_mode = fields.String(
        metadata={"description": "The standby mode: never, default"})


class PortEnabledSchema(Schema):
    tile_id = fields.Integer(
        metadata={"description": "The tile id"})
    port = fields.Integer(
        metadata={"description": "The port number"})
    enabled = fields.Integer(
        metadata={"description": "The enabled 1; disabled 0"})


class MemoryEccStateSchema(Schema):
    enabled = fields.Integer(
        metadata={"description": "The enabled 1; disabled 0"})

class ResetSchema(Schema):
    None

class PPRSchema(Schema):
    None

class PortBeaconingSchema(Schema):
    tile_id = fields.Integer(
        metadata={"description": "The tile id"})
    port = fields.Integer(
        metadata={"description": "The port number"})
    beaconing = fields.Integer(
        metadata={"description": "The beaconing on 1; off 0"})


class PerformancefactorSchema(Schema):
    tile_id = fields.Integer(
        metadata={"description": "The tile id"})
    engine = fields.String(
        metadata={"description": "engine name"})
    factor = fields.Float(
        metadata={"description": "performance factor"})


class PowerLimitSchema(Schema):
    # tile_id = fields.Integer(
    #    metadata={"description": "The tile id"})
    power_limit = fields.Integer(
        metadata={"description": "The power limit value"})
    #power_average_window = fields.Integer(
    #    metadata={"description": "The interval window"})


class FrequencySchema(Schema):
    tile_id = fields.Integer(
        metadata={"description": "The tile id"})
    min_frequency = fields.Integer(
        metadata={"description": "The min frequency value"})
    max_frequency = fields.Integer(
        metadata={"description": "The max frequency value"})


class SchedulerSchema(Schema):
    tile_id = fields.Integer(
        metadata={"description": "The tile id"})
    scheduler_mode = fields.String(
        metadata={"description": "The scheduler mode: timeout, timeslice, exclusive and debug"})
    scheduler_watchdog_timeout = fields.Integer(
        metadata={"description": "The watchdog timeout for timeout mode"})
    scheduler_timeslice_interval = fields.Integer(
        metadata={"description": "The interval for timeslice mode"})
    scheduler_timeslice_yield_timeout = fields.Integer(
        metadata={"description": "The yield timeout for timeslice mode"})


class ConfigParameterSchema(Schema):
    tile_id = fields.Integer(
        metadata={"description": "The tile id"})


class TileConfigSchema(Schema):
    tileId = fields.String(metadata={"description": "Tile id"})
    #power_average_window = fields.Integer(
    #    metadata={"description": "The interval window"})
    #power_average_window_vaild_range = fields.String(
    #    metadata={"description": "power's inteval scope"})
    min_frequency = fields.Integer(metadata={"description": "min frequency"})
    max_frequency = fields.Integer(metadata={"description": "max frequency"})
    gpu_frequency_valid_options = fields.String(
        metadata={"description": "frequency scope"})
    standby_mode = fields.String(
        metadata={"description": "The standby mode: never, default"})
    standby_mode_valid_options = fields.String(
        metadata={"description": "standby option"})
    scheduler_mode = fields.String(
        metadata={"description": "The scheduler mode: timeout, timeslice, exclusive and debug"})
    scheduler_watchdog_timeout = fields.Integer(
        metadata={"description": "scheduler timeout's value"})
    scheduler_timeslice_interval = fields.Integer(
        metadata={"description": "scheduler timeslice's interval value"})
    scheduler_timeslice_yield_timeout = fields.Integer(
        metadata={"description": "scheduler timeslice's yield value"})


class ConfigSchema(Schema):
    deviceId = fields.Integer(metadata={"description": "Device id"})
    memory_ecc_current_state = fields.String(metadata={"description": "The current state of memory ecc"})
    memory_ecc_pending_state = fields.String(metadata={"description": "The pending state of memory ecc"})
    power_limit = fields.Integer(metadata={"description": "The power limit value"})
    power_vaild_range = fields.String(metadata={"description": "power's scope"})
    tileConfigData = fields.Nested(TileConfigSchema)


def set_standby(deviceId):
    """
    Set standby mode for device
    ---
    put:
        tags:
            - "Config"
        description: Set standby mode for device
        parameters:
            -
                name: set_standby
                in: body
                description: set standby mode
                schema: StandbySchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500

    req = request.get_json()
    if ("tile_id" not in req) or ("standby_mode" not in req):
        return jsonify("json string is invalid"), 500
    tileId = req["tile_id"]
    standby = req["standby_mode"]
    if type(standby) != str:
        return jsonify("Invalid Parameter standby mode"), 500
    if type(tileId) != int:
        return jsonify("Invalid Parameter tileId"), 500

    if tileId < 0:
        return jsonify("invalid Parameter tileId"), 500

    code, message, data = stub.setStandby(
        deviceId, tileId, standby)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def set_powerlimit(deviceId):
    """
    Set power limit for device
    ---
    put:
        tags:
            - "Config"
        description: Set power limit for device
        parameters:
            -
                name: set_powerlimit
                in: body
                description: set the power limit
                schema: PowerLimitSchema
            -
                name: device_id
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500
    req = request.get_json()
    if ("power_limit" not in req) :
        return jsonify("json string is invalid"), 500
    power = req["power_limit"]
    if type(power) != int:
        return jsonify("Invalid Parameter power_limit"), 500
    if power <= 0:
        return jsonify("Invalid Parameter power_limit"), 500
    power = power * 1000

    # if type(tileId) != int:
    #    return jsonify("Invalid Parameter tileId"), 500

    # if tileId<0:
    #    return jsonify("invalid Parameter tileId"), 500

    #code, message, data = stub.setPowerLimit(deviceId, tileId, power, interval)
    code, message, data = stub.setPowerLimit(deviceId, -1, power, 0)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def set_frequencyrange(deviceId):
    """
    Set frequency range for device
    ---
    put:
        tags:
            - "Config"
        description: Set frequency range for device
        parameters:
            -
                name: set_frequencyrange
                in: body
                description: set the frequency range
                schema: FrequencySchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500
    req = request.get_json()
    if ("tile_id" not in req) or ("min_frequency" not in req) or ("max_frequency" not in req):
        return jsonify("json string is invalid"), 500
    tileId = req["tile_id"]
    minFreq = req["min_frequency"]
    maxFreq = req["max_frequency"]
    if type(tileId) != int:
        return jsonify("Invalid Parameter tileId"), 500
    if type(minFreq) != int:
        return jsonify("Invalid Parameter min_frequency"), 500
    if type(maxFreq) != int:
        return jsonify("Invalid Parameter max_frequency"), 500

    if tileId < 0 or minFreq < 0 or maxFreq < 0:
        return jsonify("invalid min_frequency or max_frequency value"), 500

    code, message, data = stub.setFrequencyRange(
        deviceId, tileId, minFreq, maxFreq)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def set_scheduler(deviceId):
    """
    Set scheduler mode for device
    ---
    put:
        tags:
            - "Config"
        description: Set scheduler mode for device
        parameters:
            -
                name: set_scheduler
                in: body
                description: set the scheduler mode
                schema: SchedulerSchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500
    req = request.get_json()
    if ("tile_id" not in req) or ("scheduler_mode" not in req):
        return jsonify("json string is invalid"), 500

    tileId = req["tile_id"]
    mode = req["scheduler_mode"]

    if mode.lower() == "timeout":
        if "scheduler_watchdog_timeout" not in req:
            return jsonify("missing parameter \"scheduler_watchdog_timeout\""), 500
        val1 = req["scheduler_watchdog_timeout"]
        val2 = 0
    elif mode.lower() == "timeslice":
        if "scheduler_timeslice_interval" not in req or "scheduler_timeslice_yield_timeout" not in req:
            return jsonify("missing parameter \"scheduler_timeslice_interval\" or \"scheduler_timeslice_yield_timeout\""), 500
        val1 = req["scheduler_timeslice_interval"]
        val2 = req["scheduler_timeslice_yield_timeout"]
    elif mode.lower() == "exclusive":
        val1 = 0
        val2 = 0
    elif mode.lower() == "debug":
        val1 = 0
        val2 = 0
    else:
        return jsonify("invalid scheduler mode"), 500

    if type(tileId) != int:
        return jsonify("Invalid Parameter tileId"), 500
    if type(mode) != str:
        return jsonify("Invalid Parameter scheduler_mode"), 500
    if type(val1) != int:
        return jsonify("Invalid Parameter type"), 500
    if type(val2) != int:
        return jsonify("Invalid Parameter type"), 500

    if tileId < 0 or val1 < 0 or val2 < 0:
        return jsonify("invalid Parameter or out of range"), 500

    code, message, data = stub.setScheduler(deviceId, tileId, mode, val1, val2)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def set_portenabled(deviceId):
    """
    Set port enabled for device
    ---
    put:
        tags:
            - "Config"
        description: Set port enabled for device
        parameters:
            -
                name: set_portenabled
                in: body
                description: set the port enabled
                schema: PortEnabledSchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500
    req = request.get_json()
    if ("tile_id" not in req) or ("port" not in req) or ("enabled" not in req):
        return jsonify("json string is invalid"), 500

    tileId = req["tile_id"]
    port = req["port"]
    enabled = req["enabled"]

    if type(tileId) != int:
        return jsonify("Invalid Parameter tileId"), 500
    if type(port) != int:
        return jsonify("Invalid Parameter port"), 500
    if type(enabled) != int:
        return jsonify("Invalid Parameter enabled"), 500

    if tileId < 0 or port < 0 or (enabled != 0 and enabled != 1):
        return jsonify("invalid Parameter value or out of range"), 500

    code, message, data = stub.setPortEnabled(deviceId, tileId, port, enabled)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def set_portbeaconing(deviceId):
    """
    Set port beaconing for device
    ---
    put:
        tags:
            - "Config"
        description: Set port beaconing for device
        parameters:
            -
                name: set_portbeaconing
                in: body
                description: set the port beaconing
                schema: PortBeaconingSchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500
    req = request.get_json()
    if ("tile_id" not in req) or ("port" not in req) or ("beaconing" not in req):
        return jsonify("json string is invalid"), 500

    tileId = req["tile_id"]
    port = req["port"]
    beaconing = req["beaconing"]

    if type(tileId) != int:
        return jsonify("Invalid Parameter tileId"), 500
    if type(port) != int:
        return jsonify("Invalid Parameter port"), 500
    if type(beaconing) != int:
        return jsonify("Invalid Parameter beaconing"), 500

    if tileId < 0 or port < 0 or (beaconing != 0 and beaconing != 1):
        return jsonify("invalid Parameter value or out of range"), 500

    code, message, data = stub.setPortBeaconing(
        deviceId, tileId, port, beaconing)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def set_performancefactor(deviceId):
    """
    Set performance factor for device
    ---
    put:
        tags:
            - "Config"
        description: Set performance factor for device
        parameters:
            -
                name: set_performancefactor
                in: body
                description: set the performance factor
                schema: PerformancefactorSchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500
    req = request.get_json()
    if ("tile_id" not in req) or ("engine" not in req) or ("factor" not in req):
        return jsonify("json string is invalid"), 500

    tileId = req["tile_id"]
    engine = req["engine"]
    factor = req["factor"]

    if type(tileId) != int:
        return jsonify("Invalid Parameter tileId"), 500
    if type(engine) != str:
        return jsonify("Invalid Parameter engine"), 500
    if type(factor) != float and type(factor) != int:
        return jsonify("Invalid Parameter factor"), 500

    if tileId < 0 or factor < 0:
        return jsonify("invalid Parameter value or out of range"), 500

    if engine.lower() == "media":
        engineValue = 1 << 3
    elif engine.lower() == "compute":
        engineValue = 1 << 1
    else:
        return jsonify("Invalid engine value"), 500

    code, message, data = stub.setPerformanceFactor(
        deviceId, tileId, engineValue, factor)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def set_memoryecc(deviceId):
    """
    Set memory ecc state for device
    ---
    put:
        tags:
            - "Config"
        description: Set memory ecc state for device
        parameters:
            -
                name: set_memoryecc
                in: body
                description: set the memory ecc state
                schema: MemoryEccStateSchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500

    req = request.get_json()
    if "enabled" not in req:
        return jsonify("json string is invalid"), 500

    enabled = req["enabled"]
    if type(enabled) != int:
        return jsonify("Invalid Parameter enabled"), 500

    if enabled != 0 and enabled != 1:
        return jsonify("Invalid value of enabled"), 500

    code, message, data = stub.setMemoryecc(deviceId, enabled)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def run_reset(deviceId):
    """
    Reset the device
    ---
    put:
        tags:
            - "Config"
        description: Reset the device
        parameters:
            -
                name: run_reset
                in: body
                description: reset the device
                schema: ResetSchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500

    req = request.get_json()
    code, message, data = stub.runReset(deviceId, True)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)

def run_ppr(deviceId):
    """
    Apply PPR to the device
    ---
    put:
        tags:
            - "Config"
        description: Apply PPR to the device
        parameters:
            -
                name: run_ppr
                in: body
                description: Apply PPR to the device
                schema: PPRSchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer
        responses:
            200:
                description: OK
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500

    req = request.get_json()

    code, message, data = stub.runPpr(deviceId, True)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)


def get_config(deviceId):
    """
    Get all configuration for device
    ---
    get:
        tags:
            - "Config"
        description: Get all configuration for device
        parameters:
            -
                name: get_config
                in: body
                description: get the configuration of device
                schema: ConfigParameterSchema
            -
                name: deviceId
                in: path
                description: Device id
                type: integer

        responses:
            200:
                description: OK
                schema: ConfigSchema
            500:
                description: Error
    """
    if not request.is_json:
        return jsonify("json string is missing"), 500
    req = request.get_json()
    if ("tile_id" not in req):
        return jsonify("json string is invalid"), 500
    tileId = req["tile_id"]
    if type(tileId) != int:
        return jsonify("Invalid Parameter tileId"), 500
    if tileId < -1:
        return jsonify("invalid Parameter tileId"), 500
    code, message, data = stub.getConfig(deviceId, tileId)
    if code != 0:
        error = dict(Status=code, Message=message)
        return jsonify(error), 500
    return jsonify(data)
