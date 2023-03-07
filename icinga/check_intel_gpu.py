#!/usr/bin/env python3
#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file check_intel_gpu.py
#

import argparse
import json
import http.client
import sys

import base64
import ssl

__version__ = '0.0.1'

host = None
port = None
# bdfaddr = None
deviceId = None
disableTLS = False
username = None
password = None

warning_threshold = None
critical_threshold = None

telemetry_type_list = [
    ("gpu_utilization", dict(key="XPUM_STATS_GPU_UTILIZATION",
     name="GPU Utilization", unit="%", convert=float)),
    ("gpu_power", dict(key="XPUM_STATS_POWER",
     name="GPU Power", unit="W", convert=float)),
    ("gpu_core_temperature", dict(key="XPUM_STATS_GPU_CORE_TEMPERATURE",
     name="GPU Core Temperature", unit="C", convert=float)),
    ("gpu_memory_temperature", dict(key="XPUM_STATS_MEMORY_TEMPERATURE",
     name="GPU Memory Temperature", unit="C", convert=float)),
    ("gpu_energy_consumed", dict(key="XPUM_STATS_ENERGY",
     name="GPU Energy Consumed", unit="J", scale=1000, convert=int, counter=True)),
    ("driver_errors", dict(key="XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS",
     name="Driver Errors", convert=int, counter=True)),
    ("cache_errors_correctable", dict(key="XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE",
     name="Cache Errors Correctable", convert=int, counter=True)),
    ("cache_errors_uncorrectable", dict(key="XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE",
     name="Cache Errors Uncorrectable", convert=int, counter=True)),
    ("gpu_memory_used", dict(key="XPUM_STATS_MEMORY_USED",
     name="GPU Memory Used", unit="MiB", scale=2**20, convert=int))
]

telemetry_type_dict = dict(telemetry_type_list)


def genBasicAuthKey(username, password):
    credentials = ('%s:%s' % (username, password))
    encoded_credentials = base64.b64encode(credentials.encode('ascii'))
    return 'Basic %s' % encoded_credentials.decode("ascii")


def http_query(url):
    if disableTLS:
        conn = http.client.HTTPConnection(host, port)
    else:
        conn = http.client.HTTPSConnection(
            host, port, context=ssl._create_unverified_context())
    headers = {
        'Authorization': genBasicAuthKey(username, password)
    }
    try:
        conn.request("GET", url, "", headers)
    except Exception as e:
        print("Critical: Connection fail")
        print(e)
        exit(2)
    res = conn.getresponse()
    raw_data = res.read().decode("utf-8")
    data = None
    if raw_data is not None:
        try:
            data = json.loads(raw_data)
            data = {k.lower(): v for k, v in data.items()}
        except Exception as e:
            pass
    if res.status != 200:
        if data is not None and "message" in data:
            print("Critical: {}".format(data["message"]))
        else:
            print("Critical: Error occurs, http status: {}, message: {}".format(
                res.status, raw_data))
        exit(2)
    return json.loads(raw_data)


def checkTelemetry():
    # url = "/rest/v1/devices"
    # data = http_query(url)
    # deviceId = None
    # for d in data['devices']:
    #     if d["pci_bdf_address"] == bdfaddr:
    #         deviceId = d['device_id']
    # if deviceId is None:
    #     print("Unknown: No device found according to the BDF Address")
    #     exit(3)
    url = "/rest/v1/devices/{}/stats".format(deviceId)
    data = http_query(url)

    ok_list = []
    warning_list = []
    critical_list = []
    unknown_list = []

    # process scale
    if "tile_level" in data:
        for tile in data["tile_level"]:
            tileId = tile["tile_id"]
            tile_data_list = tile['data_list']
            for d in tile_data_list:
                m_type = d['metrics_type']
                for k in telemetry_type_dict:
                    v = telemetry_type_dict[k]
                    if v["key"] == m_type and "scale" in v:
                        scale = v["scale"]
                        if "value" in d:
                            d['value'] /= scale
                        if "min" in d:
                            d['min'] /= scale
                        if "max" in d:
                            d['max'] /= scale
                        if "avg" in d:
                            d['avg'] /= scale
                        break
    elif "device_level" in data:
        data_list = data["device_level"]
        for d in data_list:
            m_type = d['metrics_type']
            for k in telemetry_type_dict:
                v = telemetry_type_dict[k]
                if v["key"] == m_type and "scale" in v:
                    scale = v["scale"]
                    if "value" in d:
                        d['value'] /= scale
                    if "min" in d:
                        d['min'] /= scale
                    if "max" in d:
                        d['max'] /= scale
                    if "avg" in d:
                        d['avg'] /= scale
                    break
    else:
        print("Critical: No data")
        exit(2)

    perf_data_list = []
    detail_list = []
    tile_data_dict_list = []

    if "tile_level" in data:
        for tile in data["tile_level"]:
            tileId = tile["tile_id"]
            tile_data_list = tile['data_list']
            tile_data_dict = {v["metrics_type"]: v for v in tile_data_list}
            tile_data_dict_list.append((tileId, tile_data_dict))
    else:
        tile_data_dict = {v["metrics_type"]: v for v in data["device_level"]}
        tile_data_dict_list.append((0, tile_data_dict))

    # for tile in data["tile_level"]:
    #     tileId = tile["tile_id"]
    #     tile_data_list = tile['data_list']
    #     tile_data_dict = {v["metrics_type"]: v for v in tile_data_list}
    for tileId, tile_data_dict in tile_data_dict_list:
        for k in telemetry_type_dict:
            metric_type = telemetry_type_dict[k]
            d = tile_data_dict.get(metric_type["key"], None)
            if d is None:
                detail_list.append("Tile {} {} status: Unknown".format(
                    tileId, metric_type["name"]))
                unknown_list.append(k)
            else:
                value = d['value'] if "counter" in metric_type else d["avg"]
                if k in critical_threshold and critical_threshold[k] <= value:
                    detail_list.append("Tile {} {} status: Critical ({}{} >= {}{} threshold)".format(
                        tileId,
                        metric_type["name"],
                        value,
                        metric_type.get('unit', ""),
                        critical_threshold[k],
                        metric_type.get('unit', "")))
                    critical_list.append(k)
                elif k in warning_threshold and warning_threshold[k] <= value:
                    detail_list.append("Tile {} {} status: Warning ({}{} >= {}{} threshold)".format(
                        tileId,
                        metric_type["name"],
                        value,
                        metric_type.get('unit', ""),
                        warning_threshold[k],
                        metric_type.get('unit', "")))
                    warning_list.append(k)
                else:
                    detail_list.append("Tile {} {} status: OK".format(
                        tileId, metric_type["name"]))
                    ok_list.append(k)
                if "counter" in metric_type:
                    perf_data_list.append(
                        "'Tile {} {}'={}{};{};{};;".format(
                            tileId,
                            metric_type['name'],
                            d['value'],
                            metric_type.get('unit', ""),
                            warning_threshold.get(k, ""),
                            critical_threshold.get(k, "")))
                else:
                    perf_data_list.append(
                        "'Tile {} {}'={}{};{};{};{};{}".format(
                            tileId,
                            metric_type['name'],
                            d['avg'],
                            metric_type.get('unit', ""),
                            warning_threshold.get(k, ""),
                            critical_threshold.get(k, ""),
                            d['min'],
                            d['max']))

    if len(critical_list) > 0:
        print("Critical: Some telemetry is Critical")
        exit_code = 2
    elif len(warning_list) > 0:
        print("Warning: Some telemetry is Warning")
        exit_code = 1
    elif len(ok_list) > 0:
        print("OK: Telemetry is good")
        exit_code = 0
    else:
        print("Unknown: All telemetry is Unknown")
        exit_code = 3

    print("\n".join(detail_list))
    print("| "+" ".join(perf_data_list))
    exit(exit_code)


health_type_list = [
    ("core_temperature", dict(name="GPU core temperature health")),
    ("memory", dict(name="GPU memory health")),
    ("memory_temperature", dict(name="GPU memory temperature health")),
    ("power", dict(name="GPU power health")),
    ("xe_link_port", dict(name="GPU Xe Link port health"))
]

health_type_dict = dict(health_type_list)


def checkHealth():
    # url = "/rest/v1/devices"
    # data = http_query(url)
    # deviceId = None
    # for d in data['devices']:
    #     if d["pci_bdf_address"] == bdfaddr:
    #         deviceId = d['device_id']
    # if deviceId is None:
    #     print("Unknown: No device found according to the BDF Address")
    #     exit(3)
    url = "/rest/v1/devices/{}/health".format(deviceId)
    data = http_query(url)
    ok_list = []
    warning_list = []
    critical_list = []
    unknown_list = []
    detail_list = []
    for k in health_type_dict:
        health_type = health_type_dict[k]
        status = data[k]['status']
        description = data[k]['description']
        detail_list.append("{}, status: {}, description: {}".format(
            health_type['name'], status, description))
        if status == "Critical":
            critical_list.append(k)
        elif status == "Warning":
            warning_list.append(k)
        elif status == "Unknown":
            unknown_list.append(k)
        elif status == "OK":
            ok_list.append(k)

    if len(critical_list) > 0:
        print("Critical: GPU health is Critical")
        exit_code = 2
    elif len(warning_list) > 0:
        print("Warning: GPU health is Warning")
        exit_code = 1
    elif len(ok_list) > 0:
        print("OK: GPU health is good")
        exit_code = 0
    else:
        print("Unknown: GPU health is Unknown")
        exit_code = 3

    print("\n".join(detail_list))
    exit(exit_code)


def arg():
    parser = argparse.ArgumentParser(description='Use Intel XPU Manager to check GPU telemetries and status for a remote host.')
    parser.add_argument('-V', '--version', action='version',
                        version='%(prog)s v' + sys.modules[__name__].__version__)
    parser.add_argument(
        '-H', '--host', required=True, help="The ip address or hostname of the host that runs XPU Manager Restful API service")
    parser.add_argument(
        '-p', '--port', required=True, help="The port of XPU Manager Restful API service")
    parser.add_argument(
        '-U', '--Username', help="The username of XPU Manager Restful API service")
    parser.add_argument(
        '-P', '--Password', help="The password")

    # parser.add_argument('-B', '--BDFAddr', required=True,
    #                     help="The PCI BDF Address of the gpu")
    parser.add_argument('-d', '--deviceId', required=True,
                        help="The xpum device id of the gpu")

    parser.add_argument('--disableTLS', action="store_true",
                        help="Use http instead of https")

    parser.add_argument('-T', '--Type', required=True,
                        choices=['telemetry', 'health'], help="The gpu info type to check")

    for k in telemetry_type_dict:
        v = telemetry_type_dict[k]
        parser.add_argument('--{}_warning'.format(k), type=v['convert'],
                            help='The warning threshold of {}'.format(v['name']))
        parser.add_argument('--{}_critical'.format(k), type=v['convert'],
                            help='The critical threshold of {}'.format(v['name']))

    parsed = parser.parse_args()

    global host
    global port
    # global bdfaddr
    global deviceId
    global disableTLS
    global username
    global password

    host = parsed.host
    port = parsed.port
    username = parsed.Username
    password = parsed.Password
    # bdfaddr = parsed.BDFAddr
    deviceId = parsed.deviceId
    disableTLS = parsed.disableTLS

    global warning_threshold
    global critical_threshold

    warning_threshold = dict()
    critical_threshold = dict()

    parsed_dict = vars(parsed)
    # print(parsed_dict)
    for k in telemetry_type_dict:
        v = telemetry_type_dict[k]
        warning_key = '{}_warning'.format(k)
        if warning_key in parsed_dict and parsed_dict[warning_key] is not None:
            warning_threshold[k] = parsed_dict[warning_key]
        critical_key = '{}_critical'.format(k)
        if critical_key in parsed_dict and parsed_dict[critical_key] is not None:
            critical_threshold[k] = parsed_dict[critical_key]
        if k in warning_threshold and k in critical_threshold and warning_threshold[k] >= critical_threshold[k]:
            print("Critical: {} should higher than {}".format(
                critical_key, warning_key))
            exit(2)
    # print(warning_threshold)
    # print(critical_threshold)
    return parsed


def main():
    parsed = arg()
    if parsed.Type == "telemetry":
        checkTelemetry()
    elif parsed.Type == "health":
        checkHealth()


if __name__ == "__main__":
    main()
