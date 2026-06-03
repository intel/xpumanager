#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Pre-built policy rule sets for common validation use cases.

Each function returns a list of ``PolicyRule`` objects that can be
registered with the ``PolicyEngine``.
"""

from ..events.event_types import EventType, SeverityLevel
from .engine import PolicyRule
from .actions import (
    action_cold_reset,
    action_warm_reset,
    action_firmware_update,
    action_log_critical,
    action_log_event,
    action_throttle_observed,
    action_throttle_recovered,
)


def pcode_error_recovery_rules() -> list[PolicyRule]:
    """Pcode error recovery flow: device wedged → warm reset → cold reset.

    Covers the scenario where a GPU reports a Pcode error or enters
    a wedged state.  A soft (PCIe warm) reset is attempted first;
    a cold reset is used only as an escalation if warm reset fails.
    """
    return [
        PolicyRule(
            name="pcode_error_log",
            event_types={EventType.PCODE_ERROR},
            action=action_log_critical,
        ),
        PolicyRule(
            name="device_wedged_warm_reset",
            event_types={EventType.DEVICE_WEDGED, EventType.PCODE_ERROR},
            min_severity=SeverityLevel.CRITICAL,
            action=action_warm_reset,
            cooldown_s=15.0,
            max_fires=2,
        ),
        PolicyRule(
            name="device_wedged_cold_reset",
            event_types={EventType.DEVICE_WEDGED, EventType.PCODE_ERROR},
            min_severity=SeverityLevel.CRITICAL,
            action=action_cold_reset,
            cooldown_s=30.0,
            max_fires=1,
        ),
        PolicyRule(
            name="device_wedged_recovery_check",
            event_types={EventType.HEALTH_RECOVERED},
            action=action_log_event,
        ),
    ]


def device_survivability_rules() -> list[PolicyRule]:
    """Device survivability mode: survivability event → firmware update.

    Covers the scenario where a device enters survivability mode
    (e.g. after a failed FW update or corruption), requiring a
    firmware re-flash to restore full functionality.

    The firmware image path is resolved from the PCI device ID of the
    affected device using the mapping in ``firmware_config.json``.
    """
    return [
        PolicyRule(
            name="survivability_detected",
            event_types={EventType.DEVICE_SURVIVABILITY},
            action=action_log_critical,
        ),
        PolicyRule(
            name="survivability_fw_update",
            event_types={EventType.DEVICE_SURVIVABILITY},
            min_severity=SeverityLevel.CRITICAL,
            action=action_firmware_update,
            cooldown_s=60.0,
            max_fires=1,
        ),
    ]


def ras_event_rules() -> list[PolicyRule]:
    """RAS (Reliability, Availability, Serviceability) event handling.

    Correctable errors are logged; uncorrectable errors trigger a warm
    reset. Persistent uncorrectable errors escalate to a cold reset.
    """
    return [
        PolicyRule(
            name="ras_correctable_log",
            event_types={EventType.RAS_CORRECTABLE},
            action=action_log_event,
        ),
        PolicyRule(
            name="ras_uncorrectable_warm_reset",
            event_types={EventType.RAS_UNCORRECTABLE},
            min_severity=SeverityLevel.WARNING,
            action=action_warm_reset,
            cooldown_s=15.0,
        ),
        PolicyRule(
            name="ras_uncorrectable_cold_reset",
            event_types={EventType.RAS_UNCORRECTABLE},
            min_severity=SeverityLevel.CRITICAL,
            action=action_cold_reset,
            cooldown_s=30.0,
            max_fires=2,
        ),
    ]


def thermal_throttle_rules() -> list[PolicyRule]:
    """Thermal management rules.

    The device hardware handles thermal throttling autonomously.
    Warning-level events are logged for observability; recovery is
    also logged.  Critical events escalate to a cold reset.
    """
    return [
        PolicyRule(
            name="thermal_throttle",
            event_types={EventType.THERMAL_THROTTLE, EventType.HEALTH_WARNING},
            domains={"core_thermal", "memory_thermal"},
            min_severity=SeverityLevel.WARNING,
            action=action_throttle_observed,
            cooldown_s=10.0,
        ),
        PolicyRule(
            name="thermal_critical_reset",
            event_types={EventType.THERMAL_THROTTLE, EventType.HEALTH_CRITICAL},
            domains={"core_thermal", "memory_thermal"},
            min_severity=SeverityLevel.CRITICAL,
            action=action_cold_reset,
            cooldown_s=30.0,
            max_fires=2,
        ),
        PolicyRule(
            name="thermal_recovery",
            event_types={EventType.HEALTH_RECOVERED},
            domains={"core_thermal", "memory_thermal"},
            action=action_throttle_recovered,
        ),
    ]


def power_management_rules() -> list[PolicyRule]:
    """Power budget management rules."""
    return [
        PolicyRule(
            name="power_throttle",
            event_types={EventType.POWER_THROTTLE, EventType.HEALTH_WARNING},
            domains={"power"},
            min_severity=SeverityLevel.WARNING,
            action=action_throttle_observed,
            cooldown_s=10.0,
        ),
        PolicyRule(
            name="power_critical",
            event_types={EventType.HEALTH_CRITICAL},
            domains={"power"},
            min_severity=SeverityLevel.CRITICAL,
            action=action_log_critical,
        ),
    ]


def device_lifecycle_rules() -> list[PolicyRule]:
    """Device hot-plug / hot-unplug lifecycle rules."""
    return [
        PolicyRule(
            name="device_added_log",
            event_types={EventType.DEVICE_ADDED},
            action=action_log_event,
        ),
        PolicyRule(
            name="device_removed_log",
            event_types={EventType.DEVICE_REMOVED},
            action=action_log_critical,
        ),
    ]


def group_policy_rules() -> list[PolicyRule]:
    """Group-level policy rules.

    These apply the same policy to all devices. In a real deployment,
    the ``bdfs`` field would be populated with all BDFs in the group.
    """
    return [
        PolicyRule(
            name="group_health_warning",
            event_types={
                EventType.HEALTH_WARNING,
                EventType.HEALTH_CRITICAL,
                EventType.HEALTH_FAILED,
            },
            action=action_log_critical,
        ),
        PolicyRule(
            name="group_thermal_throttle",
            event_types={EventType.THERMAL_THROTTLE},
            action=action_throttle_observed,
            cooldown_s=10.0,
        ),
        PolicyRule(
            name="group_ras_reset",
            event_types={EventType.RAS_UNCORRECTABLE},
            min_severity=SeverityLevel.CRITICAL,
            action=action_cold_reset,
            cooldown_s=30.0,
        ),
    ]


def all_default_rules() -> list[PolicyRule]:
    """Return all built-in rules combined."""
    rules: list[PolicyRule] = []
    rules.extend(pcode_error_recovery_rules())
    rules.extend(device_survivability_rules())
    rules.extend(ras_event_rules())
    rules.extend(thermal_throttle_rules())
    rules.extend(power_management_rules())
    rules.extend(device_lifecycle_rules())
    return rules
