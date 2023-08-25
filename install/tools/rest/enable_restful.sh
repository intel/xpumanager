#!/usr/bin/env bash
#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file enable_restful.sh
#

set -e
__dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

umask 022
pip3 install -r ${__dir}/rest/requirements.txt

python3 ${__dir}/rest_config.py
${__dir}/keytool.sh

systemctl start xpum_rest.service
systemctl enable xpum_rest.service
