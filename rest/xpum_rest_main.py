#!/usr/bin/python3

import configparser
import os
import sys
import hashlib
from flask import Flask
from flask_httpauth import HTTPBasicAuth

from views import versions
from views import devices
from views import exporter
from views import diagnostics
from views import health
from views import groups
from views import firmwares
from views import agent_settings
from views import statistics
from views import device_config
from views import topology
from views import dump_raw_data

app = Flask(__name__)

auth = HTTPBasicAuth()

# prometheus exporter
app.add_url_rule('/metrics',
                 view_func=exporter.export_metrics, methods=['GET'])

# version
app.add_url_rule('/rest/v1/version',
                 view_func=auth.login_required(versions.get_version), methods=['GET'])

# devices
app.add_url_rule('/rest/v1/devices',
                 view_func=auth.login_required(devices.get_devices), methods=['GET'])
app.add_url_rule('/rest/v1/devices/<int:deviceId>',
                 view_func=auth.login_required(devices.get_device_properties), methods=['GET'])

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

# groups management
app.add_url_rule('/rest/v1/groups', methods=['POST', 'GET'],
                 view_func=auth.login_required(groups.groups))
app.add_url_rule('/rest/v1/groups/<int:groupId>', methods=['GET', 'POST', 'DELETE'],
                 view_func=auth.login_required(groups.group_detail))

# firmware flash
app.add_url_rule('/rest/v1/devices/<int:deviceId>/firmwareUpgrade', methods=['POST'],
                 view_func=auth.login_required(firmwares.run_firmware_flash))
app.add_url_rule('/rest/v1/devices/<int:deviceId>/firmware', methods=['GET'],
                 view_func=auth.login_required(firmwares.get_firmware_flash_result))

# agent settings
app.add_url_rule('/rest/v1/globalSettings', methods=['GET', 'POST'],
                 view_func=auth.login_required(agent_settings.agent_setting))

# statistics
app.add_url_rule('/rest/v1/devices/<int:deviceId>/stats', methods=['GET'],
                 view_func=auth.login_required(statistics.get_statistics))
app.add_url_rule('/rest/v1/groups/<int:groupId>/stats', methods=['GET'],
                 view_func=auth.login_required(statistics.get_group_statistics))

# device config
app.add_url_rule('/rest/v1/devices/<int:deviceId>/standbys', methods=['GET', 'POST'],
                 view_func=auth.login_required(device_config.operate_device_standbys_result))
app.add_url_rule('/rest/v1/devices/<int:deviceId>/powerLimits', methods=['GET', 'POST'],
                 view_func=auth.login_required(device_config.operate_device_powerlimits_result))
app.add_url_rule('/rest/v1/devices/<int:deviceId>/frequencyRanges', methods=['GET', 'POST'],
                 view_func=auth.login_required(device_config.operate_device_frequencyranges_result))
app.add_url_rule('/rest/v1/devices/<int:deviceId>/schedulers', methods=['GET', 'POST'],
                 view_func=auth.login_required(device_config.operate_device_schedulers_result))
app.add_url_rule('/rest/v1/devices/<int:deviceId>/reset', methods=['POST'],
                 view_func=auth.login_required(device_config.operate_device_reset))

# topology
app.add_url_rule('/rest/v1/devices/<int:deviceId>/topology', methods=['GET'],
                 view_func=auth.login_required(topology.get_topology))

# dump raw data
app.add_url_rule('/rest/v1/metricsRawDataTask', methods=['POST'],
                 view_func=auth.login_required(dump_raw_data.start_metrics_raw_data_collect_task))
app.add_url_rule('/rest/v1/metricsRawDataTask/<int:taskId>', methods=['POST', 'GET'],
                 view_func=auth.login_required(dump_raw_data.metrics_raw_data_collect_task))


@auth.verify_password
def verify_password(username, password):
    if not disableAuth:
        tmpHash = hashlib.sha512((salt + password).encode('ASCII')).hexdigest()
        if username == user and tmpHash == pwHash:
            return True
        else:
            return False
    else:
        return True


def readConfig(path):
    file = path + '/rest.conf'
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
        print('No rest.conf found, must run restConfig.py first!')
        sys.exit(1)


user, salt, pwHash, disableAuth = readConfig(
    os.path.dirname(os.path.realpath(__file__)))

if __name__ == '__main__':
    app.debug = True
#   app.run()
    app.run(host='0.0.0.0', port=30000, use_reloader=False)
