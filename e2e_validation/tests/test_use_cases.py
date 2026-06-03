#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Unit tests for use-case validators.

Validators that *only* exercise policy wiring (group, thermal logging,
device lifecycle, cooldown, max-fires) run fully offline and must PASS.

Validators that exercise remediation actions requiring real
infrastructure (Pcode → cold reset, survivability → firmware update,
RAS → reset, severity escalation → reset) must report ERROR when that
infrastructure is unavailable.  Silently PASSing in that case is the
bug fixed by the strict-validation patch.
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
    def test_pcode_errors_when_xpu_smi_missing(self):
        v = PcodeErrorRecoveryValidator(xpu_smi=None)
        result = v.run()
        assert result.status == ValidationStatus.ERROR
        assert "xpu-smi" in result.message.lower()


class _FakeSmi:
    """Minimal xpu-smi stand-in: present binary, one discoverable device."""

    def discovery(self):
        return [{"device_id": 0, "pci_bdf_address": "0000:03:00.0"}]


class TestDeviceSurvivability:
    def test_survivability_errors_when_xpu_smi_missing(self):
        v = DeviceSurvivabilityValidator(xpu_smi=None)
        result = v.run()
        assert result.status == ValidationStatus.ERROR
        assert "xpu-smi" in result.message.lower()

    def test_survivability_errors_when_fw_image_missing(self, monkeypatch):
        # xpu-smi is present, but no firmware image is configured: the
        # validator must ERROR rather than PASS.
        monkeypatch.delenv("E2E_SURVIVABILITY_FW_PATH", raising=False)
        v = DeviceSurvivabilityValidator(xpu_smi=_FakeSmi())
        result = v.run()
        assert result.status == ValidationStatus.ERROR
        assert "firmware" in result.message.lower()

    def test_survivability_errors_when_fw_image_path_invalid(self, monkeypatch):
        # A configured path that does not point at a readable file is also
        # an ERROR, not a PASS.
        monkeypatch.setenv(
            "E2E_SURVIVABILITY_FW_PATH", "/nonexistent/firmware/image.bin"
        )
        v = DeviceSurvivabilityValidator(xpu_smi=_FakeSmi())
        result = v.run()
        assert result.status == ValidationStatus.ERROR
        assert "firmware" in result.message.lower()

    def test_survivability_passes_with_real_infra(self, tmp_path, monkeypatch):
        # xpu-smi present, a device is discoverable, and a readable firmware
        # image exists: the full (non-destructive) call path must PASS.
        fw = tmp_path / "fw.bin"
        fw.write_bytes(b"\x00")
        monkeypatch.setenv("E2E_SURVIVABILITY_FW_PATH", str(fw))
        v = DeviceSurvivabilityValidator(xpu_smi=_FakeSmi())
        result = v.run()
        assert result.status == ValidationStatus.PASS


class TestRASEvents:
    def test_ras_errors_when_xpu_smi_missing(self):
        v = RASEventValidator(xpu_smi=None)
        result = v.run()
        assert result.status == ValidationStatus.ERROR
        assert "xpu-smi" in result.message.lower()


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
    def test_escalation_errors_when_xpu_smi_missing(self):
        v = SeverityEscalationValidator(xpu_smi=None)
        result = v.run()
        assert result.status == ValidationStatus.ERROR
        assert "xpu-smi" in result.message.lower()


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
