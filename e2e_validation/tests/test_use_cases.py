#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Unit tests for use-case validators.

Each test runs a validator in offline mode (synthetic events, no live
gRPC or xpu-smi required). This validates the policy-rule wiring for
each use case.
"""

from e2e_validation.validators.base import ValidationStatus
from e2e_validation.validators.use_cases import (
    PcodeErrorRecoveryValidator,
    DeviceSurvivabilityValidator,
    RASEventValidator,
    GroupPolicyValidator,
    ThermalThrottleValidator,
    SeverityEscalationValidator,
    DeviceLifecycleValidator,
    PolicyCooldownValidator,
    MaxFiresValidator,
)


class TestPcodeErrorRecovery:
    def test_pcode_flow_passes(self):
        v = PcodeErrorRecoveryValidator()
        result = v.run()
        assert result.status == ValidationStatus.PASS
        assert "cold reset" in result.message.lower()


class TestDeviceSurvivability:
    def test_survivability_flow_passes(self):
        v = DeviceSurvivabilityValidator()
        result = v.run()
        assert result.status == ValidationStatus.PASS
        assert "firmware" in result.message.lower()


class TestRASEvents:
    def test_ras_flow_passes(self):
        v = RASEventValidator()
        result = v.run()
        assert result.status == ValidationStatus.PASS


class TestGroupPolicy:
    def test_group_policy_passes(self):
        v = GroupPolicyValidator()
        result = v.run()
        assert result.status == ValidationStatus.PASS
        assert "3 devices" in result.message


class TestThermalThrottle:
    def test_thermal_flow_passes(self):
        v = ThermalThrottleValidator()
        result = v.run()
        assert result.status == ValidationStatus.PASS
        assert "recovery" in result.message.lower()


class TestSeverityEscalation:
    def test_escalation_passes(self):
        v = SeverityEscalationValidator()
        result = v.run()
        assert result.status == ValidationStatus.PASS


class TestDeviceLifecycle:
    def test_lifecycle_passes(self):
        v = DeviceLifecycleValidator()
        result = v.run()
        assert result.status == ValidationStatus.PASS


class TestPolicyCooldown:
    def test_cooldown_passes(self):
        v = PolicyCooldownValidator()
        result = v.run()
        assert result.status == ValidationStatus.PASS


class TestMaxFires:
    def test_max_fires_passes(self):
        v = MaxFiresValidator()
        result = v.run()
        assert result.status == ValidationStatus.PASS
