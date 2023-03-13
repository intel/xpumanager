#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file versions.py
#

import stub
from flask import jsonify
from marshmallow import Schema, fields


class VersionInfoSchema(Schema):
    xpum_version = fields.Str(
        metadata={"description": "XPUM version"})
    xpum_version_git = fields.Str(
        metadata={"description": "The git commit hash of this build"})
    level_zero_version = fields.Str(
        metadata={"description": "Underlying level-zero lib version"})


def get_version():
    """Get version info
    ---
    get:
        tags:
            - "Version Info"
        description: Get XPU Manager version infos
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: VersionInfoSchema
            500:
                description: Error
    """
    code, message, data = stub.getVersion()
    if code == 0:
        return jsonify(data)
    else:
        return message, 500
