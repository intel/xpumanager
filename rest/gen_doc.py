#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file gen_doc.py
#

from apispec import APISpec
from apispec.ext.marshmallow import MarshmallowPlugin
from apispec_webframeworks.flask import FlaskPlugin
from flask import Flask
from marshmallow import Schema, fields
import json

from xpum_rest_main import main

app = main()


# Create an APISpec
spec = APISpec(
    title="XPU Manager Restful API",
    version="1.0.0",
    openapi_version="2.0",
    plugins=[FlaskPlugin(), MarshmallowPlugin()],
)

# Optional security scheme support
api_key_scheme = {"type": "basic"}
spec.components.security_scheme("BasicAuth", api_key_scheme)

with app.test_request_context():
    for rule in app.url_map.iter_rules():
        # print(rule.rule,rule.endpoint)
        if rule.rule.startswith("/rest/v1"):
            spec.path(view=app.view_functions.get(rule.endpoint))
    # print(json.dumps(spec.to_dict(), indent=4))
    print(spec.to_yaml())
