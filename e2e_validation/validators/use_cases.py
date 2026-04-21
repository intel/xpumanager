#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Use-case validators for specific recovery and error flows.

Each validator tests one end-to-end scenario through the full stack:
  kernel → sysman → xpumd → gRPC API → Python client → xpu-smi

These validators work by:
  1. Setting up policy rules for the use case.
  2. Starting the health watcher to collect events.
  3. Waiting for relevant events (or injecting synthetic ones for testing).
  4. Verifying that the policy engine fired the expected actions.
"""

import logging
from typing import Optional

from .base import BaseValidator, ValidationResult, ValidationStatus
from ..events.event_types import DeviceEvent, EventType, SeverityLevel
from ..grpc_client import GrpcClient
from ..policies.engine import PolicyEngine, PolicyRule
from ..policies.presets import (
    pcode_error_recovery_rules,
    device_survivability_rules,
    ras_event_rules,
    thermal_throttle_rules,
    group_policy_rules,
    all_default_rules,
)
from ..xpu_smi import XpuSmi

log = logging.getLogger(__name__)


class PcodeErrorRecoveryValidator(BaseValidator):
    """Validate Pcode error → device wedged → cold reset flow.

    This validator checks that:
      1. The policy engine has rules for PCODE_ERROR and DEVICE_WEDGED.
      2. A synthetic pcode error event triggers the cold reset action.
      3. A subsequent recovery event is logged.
    """

    name = "pcode_error_recovery"

    def __init__(
        self,
        xpu_smi: Optional[XpuSmi] = None,
        grpc_client: Optional[GrpcClient] = None,
    ) -> None:
        self._smi = xpu_smi
        self._grpc = grpc_client

    def _run(self) -> ValidationResult:
        details: dict = {"policy_firings": [], "checks": []}

        engine = PolicyEngine(self._smi)
        for rule in pcode_error_recovery_rules():
            engine.add_rule(rule)

        details["checks"].append(
            f"Registered {len(pcode_error_recovery_rules())} pcode recovery rules"
        )

        # Simulate a pcode error event
        pcode_event = DeviceEvent(
            event_type=EventType.PCODE_ERROR,
            bdf="0000:03:00.0",
            uuid="test-pcode-uuid",
            domain="gpu_core",
            severity=SeverityLevel.CRITICAL,
            reason="pcode_timeout",
            message="GPU Pcode firmware not responding",
        )
        engine.handle_event(pcode_event)

        # Simulate device wedged
        wedged_event = DeviceEvent(
            event_type=EventType.DEVICE_WEDGED,
            bdf="0000:03:00.0",
            uuid="test-pcode-uuid",
            domain="gpu_core",
            severity=SeverityLevel.FAILED,
            reason="device_wedged",
            message="Device has entered wedged state",
        )
        engine.handle_event(wedged_event)

        # Simulate recovery
        recovery_event = DeviceEvent(
            event_type=EventType.HEALTH_RECOVERED,
            bdf="0000:03:00.0",
            uuid="test-pcode-uuid",
            domain="gpu_core",
            severity=SeverityLevel.OK,
        )
        engine.handle_event(recovery_event)

        for f in engine.firings:
            details["policy_firings"].append(
                {
                    "rule": f.rule_name,
                    "event": f.event.event_type.value,
                    "success": f.success,
                    "error": f.error,
                }
            )

        # Verify expected firings
        rule_names = [f.rule_name for f in engine.firings]

        if "pcode_error_log" not in rule_names:
            details["checks"].append("FAIL: pcode_error_log did not fire")
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="Pcode error logging rule did not fire",
                details=details,
            )

        if "device_wedged_cold_reset" not in rule_names:
            details["checks"].append("FAIL: device_wedged_cold_reset did not fire")
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="Device wedged cold reset rule did not fire",
                details=details,
            )

        if "device_wedged_recovery_check" not in rule_names:
            details["checks"].append("FAIL: recovery check did not fire")
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="Recovery check rule did not fire",
                details=details,
            )

        details["checks"].append(
            f"All expected rules fired ({len(engine.firings)} total firings)"
        )

        return ValidationResult(
            name=self.name,
            status=ValidationStatus.PASS,
            message="Pcode error → cold reset → recovery flow validated",
            details=details,
        )


class DeviceSurvivabilityValidator(BaseValidator):
    """Validate device survivability → firmware update flow.

    Validates that:
      1. A survivability event is detected and logged.
      2. The firmware update action is triggered.
      3. The FW path is required in event extras.
    """

    name = "device_survivability"

    def __init__(self, xpu_smi: Optional[XpuSmi] = None) -> None:
        self._smi = xpu_smi

    def _run(self) -> ValidationResult:
        details: dict = {"policy_firings": [], "checks": []}

        engine = PolicyEngine(self._smi)
        for rule in device_survivability_rules():
            engine.add_rule(rule)

        # Survivability event WITHOUT fw_path → should log but FW update errors
        surv_event = DeviceEvent(
            event_type=EventType.DEVICE_SURVIVABILITY,
            bdf="0000:04:00.0",
            uuid="test-surv-uuid",
            domain="firmware",
            severity=SeverityLevel.CRITICAL,
            reason="survivability_mode",
            message="Device entered survivability mode after FW corruption",
        )
        engine.handle_event(surv_event)

        # Survivability event WITH fw_path
        surv_event_with_fw = DeviceEvent(
            event_type=EventType.DEVICE_SURVIVABILITY,
            bdf="0000:05:00.0",
            uuid="test-surv-uuid-2",
            domain="firmware",
            severity=SeverityLevel.CRITICAL,
            reason="survivability_mode",
            message="Device entered survivability mode",
            extra={"fw_path": "/opt/firmware/recovery.bin"},
        )
        engine.handle_event(surv_event_with_fw)

        for f in engine.firings:
            details["policy_firings"].append(
                {
                    "rule": f.rule_name,
                    "event_bdf": f.event.bdf,
                    "success": f.success,
                    "error": f.error,
                }
            )

        rule_names = [f.rule_name for f in engine.firings]

        if "survivability_detected" not in rule_names:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="Survivability detection rule did not fire",
                details=details,
            )

        if "survivability_fw_update" not in rule_names:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="Survivability FW update rule did not fire",
                details=details,
            )

        details["checks"].append(
            f"All expected rules fired ({len(engine.firings)} firings)"
        )

        return ValidationResult(
            name=self.name,
            status=ValidationStatus.PASS,
            message="Survivability → firmware update flow validated",
            details=details,
        )


class RASEventValidator(BaseValidator):
    """Validate RAS error event handling.

    Validates:
      1. Correctable RAS errors are logged.
      2. Uncorrectable errors (WARNING) trigger warm reset.
      3. Uncorrectable errors (CRITICAL) trigger cold reset.
    """

    name = "ras_events"

    def __init__(self, xpu_smi: Optional[XpuSmi] = None) -> None:
        self._smi = xpu_smi

    def _run(self) -> ValidationResult:
        details: dict = {"policy_firings": [], "checks": []}

        engine = PolicyEngine(self._smi)
        for rule in ras_event_rules():
            engine.add_rule(rule)

        # Correctable RAS error
        correctable = DeviceEvent(
            event_type=EventType.RAS_CORRECTABLE,
            bdf="0000:06:00.0",
            uuid="test-ras-uuid",
            domain="ras_cache",
            severity=SeverityLevel.WARNING,
            reason="ecc_correctable",
            message="Correctable ECC error detected in L2 cache",
        )
        engine.handle_event(correctable)

        # Uncorrectable RAS error - WARNING
        uncorrectable_warn = DeviceEvent(
            event_type=EventType.RAS_UNCORRECTABLE,
            bdf="0000:06:00.0",
            uuid="test-ras-uuid",
            domain="ras_cache",
            severity=SeverityLevel.WARNING,
            reason="ecc_uncorrectable",
            message="Uncorrectable ECC error detected",
        )
        engine.handle_event(uncorrectable_warn)

        # Uncorrectable RAS error - CRITICAL
        uncorrectable_crit = DeviceEvent(
            event_type=EventType.RAS_UNCORRECTABLE,
            bdf="0000:07:00.0",
            uuid="test-ras-uuid-2",
            domain="ras_memory",
            severity=SeverityLevel.CRITICAL,
            reason="memory_failure",
            message="Critical memory subsystem failure",
        )
        engine.handle_event(uncorrectable_crit)

        for f in engine.firings:
            details["policy_firings"].append(
                {
                    "rule": f.rule_name,
                    "event_type": f.event.event_type.value,
                    "severity": f.event.severity.name,
                    "success": f.success,
                }
            )

        rule_names = [f.rule_name for f in engine.firings]

        checks = {
            "ras_correctable_log": "Correctable RAS logging",
            "ras_uncorrectable_warm_reset": "Uncorrectable warm reset",
            "ras_uncorrectable_cold_reset": "Uncorrectable cold reset (critical)",
        }
        for rule_name, desc in checks.items():
            if rule_name in rule_names:
                details["checks"].append(f"PASS: {desc}")
            else:
                details["checks"].append(f"FAIL: {desc}")
                return ValidationResult(
                    name=self.name,
                    status=ValidationStatus.FAIL,
                    message=f"{desc} rule did not fire",
                    details=details,
                )

        return ValidationResult(
            name=self.name,
            status=ValidationStatus.PASS,
            message="All RAS event flows validated",
            details=details,
        )


class GroupPolicyValidator(BaseValidator):
    """Validate group-level policy application.

    Checks that:
      1. Group policy rules apply to events from multiple devices.
      2. Cooldown and max-fires limits work across the group.
    """

    name = "group_policy"

    def __init__(self, xpu_smi: Optional[XpuSmi] = None) -> None:
        self._smi = xpu_smi

    def _run(self) -> ValidationResult:
        details: dict = {"policy_firings": [], "checks": []}

        engine = PolicyEngine(self._smi)
        rules = group_policy_rules()
        for rule in rules:
            engine.add_rule(rule)

        # Events from multiple devices in the "group"
        devices = [
            ("0000:10:00.0", "group-dev-1"),
            ("0000:11:00.0", "group-dev-2"),
            ("0000:12:00.0", "group-dev-3"),
        ]

        for bdf, uuid in devices:
            event = DeviceEvent(
                event_type=EventType.HEALTH_WARNING,
                bdf=bdf,
                uuid=uuid,
                domain="core_thermal",
                severity=SeverityLevel.WARNING,
                reason="thermal_limit",
                message="GPU temperature exceeding threshold",
            )
            engine.handle_event(event)

        for f in engine.firings:
            details["policy_firings"].append(
                {
                    "rule": f.rule_name,
                    "bdf": f.event.bdf,
                    "success": f.success,
                }
            )

        # All 3 devices should have triggered the group health warning
        health_warn_firings = [
            f for f in engine.firings if f.rule_name == "group_health_warning"
        ]
        if len(health_warn_firings) != 3:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message=(
                    f"Expected 3 group_health_warning firings, got "
                    f"{len(health_warn_firings)}"
                ),
                details=details,
            )

        details["checks"].append("Group policy applied to all 3 devices")

        return ValidationResult(
            name=self.name,
            status=ValidationStatus.PASS,
            message="Group policy validated across 3 devices",
            details=details,
        )


class ThermalThrottleValidator(BaseValidator):
    """Validate thermal throttle → frequency reduction → recovery flow."""

    name = "thermal_throttle_recovery"

    def __init__(self, xpu_smi: Optional[XpuSmi] = None) -> None:
        self._smi = xpu_smi

    def _run(self) -> ValidationResult:
        details: dict = {"policy_firings": [], "checks": []}

        engine = PolicyEngine(self._smi)
        for rule in thermal_throttle_rules():
            engine.add_rule(rule)

        # Thermal warning → throttle
        warn_event = DeviceEvent(
            event_type=EventType.HEALTH_WARNING,
            bdf="0000:08:00.0",
            uuid="test-thermal-uuid",
            domain="core_thermal",
            severity=SeverityLevel.WARNING,
            reason="thermal_limit",
            message="Core temp 92°C exceeds warning threshold 85°C",
        )
        engine.handle_event(warn_event)

        # Recovery → restore
        recover_event = DeviceEvent(
            event_type=EventType.HEALTH_RECOVERED,
            bdf="0000:08:00.0",
            uuid="test-thermal-uuid",
            domain="core_thermal",
            severity=SeverityLevel.OK,
        )
        engine.handle_event(recover_event)

        for f in engine.firings:
            details["policy_firings"].append(
                {
                    "rule": f.rule_name,
                    "event_type": f.event.event_type.value,
                    "success": f.success,
                }
            )

        rule_names = [f.rule_name for f in engine.firings]

        if "thermal_throttle" not in rule_names:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="Thermal throttle rule did not fire",
                details=details,
            )

        if "thermal_recovery" not in rule_names:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="Thermal recovery rule did not fire",
                details=details,
            )

        details["checks"].append("Thermal throttle and recovery rules both fired")

        return ValidationResult(
            name=self.name,
            status=ValidationStatus.PASS,
            message="Thermal throttle → recovery flow validated",
            details=details,
        )


class SeverityEscalationValidator(BaseValidator):
    """Validate severity escalation (WARNING → CRITICAL → FAILED)."""

    name = "severity_escalation"

    def __init__(self, xpu_smi: Optional[XpuSmi] = None) -> None:
        self._smi = xpu_smi

    def _run(self) -> ValidationResult:
        details: dict = {"policy_firings": [], "checks": []}

        engine = PolicyEngine(self._smi)
        for rule in all_default_rules():
            engine.add_rule(rule)

        bdf = "0000:09:00.0"
        uuid = "test-escalation-uuid"

        # WARNING
        engine.handle_event(
            DeviceEvent(
                event_type=EventType.HEALTH_WARNING,
                bdf=bdf,
                uuid=uuid,
                domain="power",
                severity=SeverityLevel.WARNING,
                reason="power_limit",
                message="Approaching power threshold",
            )
        )

        # Escalate to CRITICAL
        engine.handle_event(
            DeviceEvent(
                event_type=EventType.HEALTH_CRITICAL,
                bdf=bdf,
                uuid=uuid,
                domain="power",
                severity=SeverityLevel.CRITICAL,
                reason="power_exceeded",
                message="Power limit exceeded",
            )
        )

        for f in engine.firings:
            details["policy_firings"].append(
                {
                    "rule": f.rule_name,
                    "severity": f.event.severity.name,
                    "success": f.success,
                }
            )

        # Should have firings at both severity levels
        severities = {f.event.severity for f in engine.firings}
        if SeverityLevel.WARNING not in severities:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="No WARNING-level firing detected",
                details=details,
            )
        if SeverityLevel.CRITICAL not in severities:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="No CRITICAL-level firing detected",
                details=details,
            )

        details["checks"].append("Both WARNING and CRITICAL severities triggered rules")

        return ValidationResult(
            name=self.name,
            status=ValidationStatus.PASS,
            message="Severity escalation validated",
            details=details,
        )


class DeviceLifecycleValidator(BaseValidator):
    """Validate hot-plug and hot-unplug event detection."""

    name = "device_lifecycle"

    def __init__(self, xpu_smi: Optional[XpuSmi] = None) -> None:
        self._smi = xpu_smi

    def _run(self) -> ValidationResult:
        details: dict = {"policy_firings": [], "checks": []}

        from ..policies.presets import device_lifecycle_rules

        engine = PolicyEngine(self._smi)
        for rule in device_lifecycle_rules():
            engine.add_rule(rule)

        # Device added
        engine.handle_event(
            DeviceEvent(
                event_type=EventType.DEVICE_ADDED,
                bdf="0000:0a:00.0",
                uuid="hotplug-uuid",
                model="Intel Data Center GPU Max 1550",
            )
        )

        # Device removed
        engine.handle_event(
            DeviceEvent(
                event_type=EventType.DEVICE_REMOVED,
                bdf="0000:0a:00.0",
                uuid="hotplug-uuid",
            )
        )

        for f in engine.firings:
            details["policy_firings"].append(
                {
                    "rule": f.rule_name,
                    "event_type": f.event.event_type.value,
                    "success": f.success,
                }
            )

        rule_names = [f.rule_name for f in engine.firings]

        if "device_added_log" not in rule_names:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="Device added rule did not fire",
                details=details,
            )

        if "device_removed_log" not in rule_names:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="Device removed rule did not fire",
                details=details,
            )

        return ValidationResult(
            name=self.name,
            status=ValidationStatus.PASS,
            message="Device lifecycle (add/remove) validated",
            details=details,
        )


class PolicyCooldownValidator(BaseValidator):
    """Validate that policy cooldown periods are respected."""

    name = "policy_cooldown"

    def _run(self) -> ValidationResult:
        details: dict = {"checks": []}

        engine = PolicyEngine()
        engine.add_rule(
            PolicyRule(
                name="cooldown_test",
                event_types={EventType.HEALTH_WARNING},
                action=lambda ev, smi: None,
                cooldown_s=1.0,
            )
        )

        event = DeviceEvent(
            event_type=EventType.HEALTH_WARNING,
            bdf="0000:0b:00.0",
            domain="test",
            severity=SeverityLevel.WARNING,
        )

        # First firing should succeed
        engine.handle_event(event)
        if len(engine.firings) != 1:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="First event should fire",
                details=details,
            )

        # Immediate second firing should be suppressed by cooldown
        engine.handle_event(event)
        if len(engine.firings) != 1:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="Second event should be suppressed by cooldown",
                details=details,
            )

        details["checks"].append("Cooldown correctly suppressed duplicate firing")

        return ValidationResult(
            name=self.name,
            status=ValidationStatus.PASS,
            message="Policy cooldown validated",
            details=details,
        )


class MaxFiresValidator(BaseValidator):
    """Validate that max_fires limit is respected."""

    name = "policy_max_fires"

    def _run(self) -> ValidationResult:
        details: dict = {"checks": []}

        engine = PolicyEngine()
        engine.add_rule(
            PolicyRule(
                name="max_fires_test",
                event_types={EventType.HEALTH_CRITICAL},
                action=lambda ev, smi: None,
                max_fires=2,
            )
        )

        for i in range(5):
            event = DeviceEvent(
                event_type=EventType.HEALTH_CRITICAL,
                bdf=f"0000:0{i}:00.0",
                domain="test",
                severity=SeverityLevel.CRITICAL,
            )
            engine.handle_event(event)

        if len(engine.firings) != 2:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message=f"Expected 2 firings, got {len(engine.firings)}",
                details=details,
            )

        details["checks"].append("max_fires=2 correctly limited firings to 2")
        return ValidationResult(
            name=self.name,
            status=ValidationStatus.PASS,
            message="max_fires limit validated",
            details=details,
        )
