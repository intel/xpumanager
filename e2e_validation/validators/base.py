#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Base validator class and result types.

Each validator tests one aspect of the kernel → sysman → xpumd → local API
→ python script → xpu-smi pipeline and reports a structured result.
"""

import enum
import logging
import time
from dataclasses import dataclass, field

log = logging.getLogger(__name__)


class ValidationStatus(enum.Enum):
    PASS = "PASS"
    FAIL = "FAIL"
    SKIP = "SKIP"
    ERROR = "ERROR"


@dataclass
class ValidationResult:
    """Outcome of a single validation check."""

    name: str
    status: ValidationStatus
    message: str = ""
    details: dict = field(default_factory=dict)
    duration_s: float = 0.0

    def __str__(self) -> str:
        s = f"[{self.status.value}] {self.name}"
        if self.message:
            s += f": {self.message}"
        return s


class BaseValidator:
    """Abstract base for all validators.

    Subclasses must implement ``_run()`` and set ``self.name``.
    """

    name: str = "BaseValidator"

    def run(self) -> ValidationResult:
        """Execute the validator and return a result."""
        t0 = time.monotonic()
        try:
            result = self._run()
        except Exception as exc:
            log.exception("Validator %s raised an exception", self.name)
            result = ValidationResult(
                name=self.name,
                status=ValidationStatus.ERROR,
                message=str(exc),
            )
        result.duration_s = time.monotonic() - t0
        log.info("%s (%.2fs)", result, result.duration_s)
        return result

    def _run(self) -> ValidationResult:
        raise NotImplementedError
