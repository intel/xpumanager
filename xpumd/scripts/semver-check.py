#!/usr/bin/python3
#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

import os
import re
import sys


def error_exit(msg):
    stderr = sys.stderr.write
    stderr(f"\n{msg}\n\n")
    sys.exit(1)

# regex from: https://semver.org/
semver=r"^(?P<major>0|[1-9]\d*)\.(?P<minor>0|[1-9]\d*)\.(?P<patch>0|[1-9]\d*)(?:-(?P<prerelease>(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+(?P<buildmetadata>[0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$"
check = re.compile(semver)

if len(sys.argv) != 2:
    error_exit(f"Usage: {os.path.basename(sys.argv[0])} <version-to-check>")
if not check.match(sys.argv[1]):
    error_exit(f"ERROR: '{sys.argv[1]}' is NOT a valid semver (see: https://semver.org/)!")
