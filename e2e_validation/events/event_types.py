#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Event type definitions for the E2E validation framework.

Each event represents a state change detected by the health watcher or
injected by a test harness.  Policies consume these events to decide
which remediation action to execute.
"""

import enum
import time
from dataclasses import dataclass, field
from typing import Any


class EventType(enum.Enum):
    """Categories of events that flow through the validation pipeline."""

    # Device lifecycle
    DEVICE_DISCOVERED = "device_discovered"
    DEVICE_ADDED = "device_added"
    DEVICE_REMOVED = "device_removed"

    # Health state changes
    HEALTH_OK = "health_ok"
    HEALTH_WARNING = "health_warning"
    HEALTH_CRITICAL = "health_critical"
    HEALTH_FAILED = "health_failed"
    HEALTH_RECOVERED = "health_recovered"

    # Specific error flows
    PCODE_ERROR = "pcode_error"
    DEVICE_WEDGED = "device_wedged"
    DEVICE_SURVIVABILITY = "device_survivability"
    RAS_CORRECTABLE = "ras_correctable"
    RAS_UNCORRECTABLE = "ras_uncorrectable"

    # Throttle events
    FREQUENCY_THROTTLE = "frequency_throttle"
    POWER_THROTTLE = "power_throttle"
    THERMAL_THROTTLE = "thermal_throttle"


class SeverityLevel(enum.IntEnum):
    """Mirror of the proto SeverityLevel, usable without proto imports."""

    UNKNOWN = 0
    OK = 1
    WARNING = 2
    CRITICAL = 3
    FAILED = 4


@dataclass(frozen=True)
class DeviceEvent:
    """An immutable event record emitted by the health watcher.

    Attributes:
        event_type:  The category of this event.
        bdf:         PCI Bus-Device-Function address.
        uuid:        Device UUID (may be empty for synthetic events).
        domain:      Health domain name (e.g. "memory", "core_thermal").
        severity:    Severity level at the time of the event.
        reason:      Short reason code (from the proto ``HealthStatus``).
        message:     Human-readable description.
        model:       Device model string (populated for discovery events).
        timestamp:   Unix timestamp when the event was created.
        extra:       Arbitrary key-value metadata for use-case-specific data.
    """

    event_type: EventType
    bdf: str = ""
    uuid: str = ""
    domain: str = ""
    severity: SeverityLevel = SeverityLevel.UNKNOWN
    reason: str = ""
    message: str = ""
    model: str = ""
    timestamp: float = field(default_factory=time.time)
    extra: dict[str, Any] = field(default_factory=dict)

    def __str__(self) -> str:
        parts = [f"[{self.event_type.value}]", f"bdf={self.bdf}"]
        if self.domain:
            parts.append(f"domain={self.domain}")
        if self.severity != SeverityLevel.UNKNOWN:
            parts.append(f"severity={self.severity.name}")
        if self.reason:
            parts.append(f"reason={self.reason}")
        if self.message:
            parts.append(f"msg={self.message!r}")
        return " ".join(parts)
