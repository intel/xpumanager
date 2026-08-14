#!/usr/bin/env python3
#
# Copyright (C) 2026 Intel Corporation
# SPDX-License-Identifier: MIT
# @file xpum_rest_main.py
#

import configparser
import os
import sys
import hashlib
import hmac
import threading
import time
from flask import Flask, request, send_from_directory
from flask_httpauth import HTTPBasicAuth

from views import versions
from views import devices
from views import exporter
from views import diagnostics
from views import health
from views import policy
from views import groups
from views import firmwares
from views import agent_settings
from views import statistics
from views import device_config
from views import topology
from views import ps
from views import dump_raw_data
from views import sensor
from views import vgpu

import xpum_logger as logger

import argparse

from stub import dump_raw_data as dump_raw_data_stub
from stub import grpc_stub

auth = HTTPBasicAuth()

env_exporter_no_auth = True if os.environ.get(
    'XPUM_EXPORTER_NO_AUTH', '') == '1' else False
env_exporter_only = True if os.environ.get(
    'XPUM_EXPORTER_ONLY', '') == '1' else False

# Per-client lockout state keyed by remote_addr: {'count': int, 'until': float}
_login_failure_lock = threading.Lock()
_login_failures: dict = {}


def main(*args, **kwargs):

    if "dump_folder" in kwargs:
        dump_raw_data_stub.dump_folder = kwargs["dump_folder"]

    if "gunicorn_pid_file" in kwargs:
        grpc_stub.gunicorn_pid_file = kwargs["gunicorn_pid_file"]

    app = Flask(__name__)

    app.url_map.strict_slashes = False

    def _download_file(filename):
        base = os.path.realpath(dump_raw_data_stub.dump_folder)
        requested = os.path.realpath(os.path.join(base, filename))
        if not requested.startswith(base + os.sep) or not os.path.isfile(requested):
            return "Not Found", 404
        return send_from_directory(base, os.path.relpath(requested, base))

    app.add_url_rule(
        dump_raw_data_stub.url_prefix + '/<path:filename>',
        view_func=auth.login_required(_download_file),
        methods=['GET']
    )

    # version
    app.add_url_rule('/rest/v1/version',
                     view_func=versions.get_version, methods=['GET'])

    # prometheus exporter
    if env_exporter_no_auth:
        app.add_url_rule(
            '/metrics', view_func=exporter.export_metrics, methods=['GET'])
        app.add_url_rule(
            '/healtz', view_func=exporter.check_health, methods=['GET'])
    else:
        app.add_url_rule(
            '/metrics', view_func=auth.login_required(exporter.export_metrics), methods=['GET'])
        app.add_url_rule(
            '/healtz', view_func=auth.login_required(exporter.check_health), methods=['GET'])

    # only enable exporter?
    if env_exporter_only:
        return app

    # devices
    app.add_url_rule('/rest/v1/devices',
                     view_func=auth.login_required(devices.get_devices), methods=['GET'])
    app.add_url_rule('/rest/v1/devices/<int:deviceId>',
                    view_func=auth.login_required(devices.get_device_properties), methods=['GET'])
    app.add_url_rule('/rest/v1/devices/amcversions',
                     view_func=auth.login_required(devices.get_amc_fw_versions), methods=['GET'])

    # diagnostics
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/diagnostics', methods=['POST'],
                     view_func=auth.login_required(diagnostics.run_diagnostics))
    app.add_url_rule('/rest/v1/groups/<int:groupId>/diagnostics', methods=['POST'],
                     view_func=auth.login_required(diagnostics.run_group_diagnostics))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/diagnostics', methods=['GET'],
                     view_func=auth.login_required(diagnostics.get_diagnostics_result))
    app.add_url_rule('/rest/v1/groups/<int:groupId>/diagnostics', methods=['GET'],
                     view_func=auth.login_required(diagnostics.get_group_diagnostics_result))

    # health
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/health', methods=['GET'],
                     view_func=auth.login_required(health.get_health_all))
    app.add_url_rule('/rest/v1/groups/<int:groupId>/health', methods=['GET'],
                     view_func=auth.login_required(health.get_group_health_all))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/health/<healthType>', methods=['GET'],
                     view_func=auth.login_required(health.get_health))
    app.add_url_rule('/rest/v1/groups/<int:groupId>/health/<healthType>', methods=['GET'],
                     view_func=auth.login_required(health.get_group_health))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/health/<healthType>', methods=['PUT'],
                     view_func=auth.login_required(health.set_health_config))
    app.add_url_rule('/rest/v1/groups/<int:groupId>/health/<healthType>', methods=['PUT'],
                     view_func=auth.login_required(health.set_group_health_config))

    # policy management
    app.add_url_rule('/rest/v1/policy', methods=['GET'],
                     view_func=auth.login_required(policy.get_all_policy))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/policy', methods=['GET'],
                     view_func=auth.login_required(policy.get_device_policy))
    app.add_url_rule('/rest/v1/groups/<int:groupId>/policy', methods=['GET'],
                     view_func=auth.login_required(policy.get_group_policy))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/policy', methods=['POST', 'DELETE'],
                     view_func=auth.login_required(policy.set_device_policy))
    app.add_url_rule('/rest/v1/groups/<int:groupId>/policy', methods=['POST', 'DELETE'],
                     view_func=auth.login_required(policy.set_group_policy))

    # groups management
    app.add_url_rule('/rest/v1/groups', methods=['POST', 'GET'],
                     view_func=auth.login_required(groups.groups))
    app.add_url_rule('/rest/v1/groups/<int:groupId>', methods=['GET', 'POST', 'DELETE'],
                     view_func=auth.login_required(groups.group_detail))

    # firmware flash
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/updatefw', methods=['POST'],
                     view_func=auth.login_required(firmwares.run_firmware_flash_single))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/firmware', methods=['GET'],
                     view_func=auth.login_required(firmwares.get_firmware_flash_result_single))
    app.add_url_rule('/rest/v1/devices/updatefw', methods=['POST'],
                     view_func=auth.login_required(firmwares.run_firmware_flash_all))
    app.add_url_rule('/rest/v1/devices/firmware', methods=['GET'],
                     view_func=auth.login_required(firmwares.get_firmware_flash_result_all))

    # agent settings
    app.add_url_rule('/rest/v1/agentSettings', methods=['GET', 'POST'],
                     view_func=auth.login_required(agent_settings.agent_setting))

    # statistics
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/stats', methods=['GET'],
                     view_func=auth.login_required(statistics.get_statistics))
    app.add_url_rule('/rest/v1/groups/<int:groupId>/stats', methods=['GET'],
                     view_func=auth.login_required(statistics.get_group_statistics))

    # device config
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/standby', methods=['PUT'],
                     view_func=auth.login_required(device_config.set_standby))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/powerlimit', methods=['PUT'],
                     view_func=auth.login_required(device_config.set_powerlimit))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/frequencyrange', methods=['PUT'],
                     view_func=auth.login_required(device_config.set_frequencyrange))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/scheduler', methods=['PUT'],
                     view_func=auth.login_required(device_config.set_scheduler))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/config', methods=['GET'],
                     view_func=auth.login_required(device_config.get_config))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/performancefactor', methods=['PUT'],
                     view_func=auth.login_required(device_config.set_performancefactor))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/reset', methods=['POST'],
                    view_func=auth.login_required(device_config.run_reset))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/ppr', methods=['POST'],
                    view_func=auth.login_required(device_config.run_ppr))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/portenabled', methods=['PUT'],
                     view_func=auth.login_required(device_config.set_portenabled))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/portbeaconing', methods=['PUT'],
                     view_func=auth.login_required(device_config.set_portbeaconing))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/memoryecc', methods=['PUT'],
                    view_func=auth.login_required(device_config.set_memoryecc))

    # topology
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/topology', methods=['GET'],
                     view_func=auth.login_required(topology.get_topology))
    app.add_url_rule('/rest/v1/topology', methods=['GET'],
                     view_func=auth.login_required(topology.export_topology))
    app.add_url_rule('/rest/v1/topology/xelink', methods=['GET'],
                     view_func=auth.login_required(topology.get_topo_xelink))

    # ps
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/ps', methods=['GET'],
                     view_func=auth.login_required(ps.get_device_util_by_proc))
    app.add_url_rule('/rest/v1/ps', methods=['GET'],
                     view_func=auth.login_required(ps.get_all_device_util_by_proc))

    # dump raw data
    app.add_url_rule('/rest/v1/dump', methods=['POST'],
                     view_func=auth.login_required(dump_raw_data.startDumpRawDataTask))
    app.add_url_rule('/rest/v1/dump/<int:taskId>', methods=['DELETE'],
                     view_func=auth.login_required(dump_raw_data.stopDumpRawDataTask))
    app.add_url_rule('/rest/v1/dump', methods=['GET'],
                     view_func=auth.login_required(dump_raw_data.listDumpRawDataTasks))

    # sensor reading
    app.add_url_rule('/rest/v1/sensor', methods=['GET'],
                     view_func=auth.login_required(sensor.getAMCSensorReading))

    # vgpu
    app.add_url_rule('/rest/v1/vgpu/precheck', methods=['GET'],
                     view_func=auth.login_required(vgpu.doVgpuPrecheck))
    app.add_url_rule('/rest/v1/vgpu/createvf', methods=['POST'],
                     view_func=auth.login_required(vgpu.createVf))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/vgpulist', methods=['GET'],
                     view_func=auth.login_required(vgpu.listVf))
    app.add_url_rule('/rest/v1/vgpu/removevf', methods=['POST'],
                     view_func=auth.login_required(vgpu.removeAllVf))
    app.add_url_rule('/rest/v1/devices/<int:deviceId>/vgpustats', methods=['GET'],
                     view_func=auth.login_required(vgpu.stats))
    
    return app


@auth.verify_password
def verify_password(username, password):
    # store username in threadlocal for audit log
    logger.set_audit_username(username)
    if not disableAuth:
        client = request.remote_addr
        now = time.monotonic()

        # Check per-client lockout without blocking the worker thread
        with _login_failure_lock:
            failure_info = _login_failures.get(client, {'count': 0, 'until': 0.0})
            if now < failure_info['until']:
                return False

        tmpHash = hashlib.pbkdf2_hmac('sha512', password.encode(
            'ASCII'), salt.encode('ASCII'), 10000).hex()
        if hmac.compare_digest(username, user) and hmac.compare_digest(tmpHash, pwHash):
            with _login_failure_lock:
                _login_failures.pop(client, None)
            return True
        else:
            with _login_failure_lock:
                failure_info = _login_failures.get(client, {'count': 0, 'until': 0.0})
                count = min(failure_info['count'] + 1, 8)
                _login_failures[client] = {
                    'count': count,
                    'until': time.monotonic() + (2 ** count),
                }

            logger.audit('Authentication', 'Failed',
                         "The username '{}' doesn't exist or the password is incorrect", username)
            return False
    else:
        return True


def read_config(path):
    file = path + '/conf/rest.conf'
    config = configparser.ConfigParser()
    if config.read(file):
        user = config['default']['username']
        salt = config['default']['salt']
        pwHash = config['default']['password']

        disableAuth = False

        if config['default'].get('disableAuth'):
            if config['default']['disableAuth'].upper() == 'TRUE':
                disableAuth = True

        return user, salt, pwHash, disableAuth
    else:
        print('No rest.conf found, must run rest_config.py first!')
        sys.exit(1)


if not env_exporter_no_auth:
    user, salt, pwHash, disableAuth = read_config(
        os.path.dirname(os.path.realpath(__file__)))

if not env_exporter_only:
    policy.startPolicyCallBackThread()

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--dump_folder",
                        help="folder stores dump raw data output files")
    args = parser.parse_args()
    if args.dump_folder:
        app = main(dump_folder=args.dump_folder)
    else:
        app = main()
    app.debug = True
    app.run(host='0.0.0.0', port=30000, ssl_context=('conf/cert.pem', 'conf/key.pem'), use_reloader=False)
