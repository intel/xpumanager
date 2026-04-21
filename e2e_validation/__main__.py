#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""Allow running as ``python -m e2e_validation``."""

from .run import main
import sys

sys.exit(main())
