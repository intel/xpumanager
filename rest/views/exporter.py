#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file exporter.py
#

from stub import devices
from flask import Response
import stub

import sys
import os

rest_folder = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

sys.path.insert(1, os.path.join(rest_folder, "prometheus_exporter"))
from prometheus_exporter import get_metrics # this import must follow sys.path.insert()


def export_metrics():
    if os.environ.get("XPUM_EXPORTER_POD") == '1':
        from kube_pod_resource import get_pod_resources
        result = get_metrics(stub, get_pod_resources())
    else:
        result = get_metrics(stub, {})

    # Handle (body, status) or just body
    if isinstance(result, tuple):
        body, status = result
    else:
        body, status = result, 200

    return Response(body, status=status, content_type='text/plain')


def check_health():
    code, _, data = devices.getDeviceList()
    if code != 0:
        return f'failed to get devices ({code})', 500

    if not data:
        return f'cannot get any devices', 500

    return 'healthy'
