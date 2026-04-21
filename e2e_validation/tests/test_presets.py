#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Unit tests for the preset policy rule sets.

Verifies that each preset returns the expected rules with correct
event types, severities, and action wiring.
"""

from e2e_validation.events.event_types import SeverityLevel
from e2e_validation.policies.presets import (
    pcode_error_recovery_rules,
    device_survivability_rules,
    ras_event_rules,
    thermal_throttle_rules,
    power_management_rules,
    device_lifecycle_rules,
    group_policy_rules,
    all_default_rules,
)


class TestPcodePreset:
    def test_has_required_rules(self):
        rules = pcode_error_recovery_rules()
        names = {r.name for r in rules}
        assert "pcode_error_log" in names
        assert "device_wedged_cold_reset" in names
        assert "device_wedged_recovery_check" in names

    def test_cold_reset_has_cooldown(self):
        rules = {r.name: r for r in pcode_error_recovery_rules()}
        reset = rules["device_wedged_cold_reset"]
        assert reset.cooldown_s > 0
        assert reset.max_fires > 0

    def test_cold_reset_requires_critical(self):
        rules = {r.name: r for r in pcode_error_recovery_rules()}
        reset = rules["device_wedged_cold_reset"]
        assert reset.min_severity >= SeverityLevel.CRITICAL


class TestSurvivabilityPreset:
    def test_has_detection_and_update(self):
        rules = device_survivability_rules()
        names = {r.name for r in rules}
        assert "survivability_detected" in names
        assert "survivability_fw_update" in names

    def test_fw_update_limited(self):
        rules = {r.name: r for r in device_survivability_rules()}
        fw = rules["survivability_fw_update"]
        assert fw.max_fires == 1


class TestRASPreset:
    def test_has_correctable_and_uncorrectable(self):
        rules = ras_event_rules()
        names = {r.name for r in rules}
        assert "ras_correctable_log" in names
        assert "ras_uncorrectable_warm_reset" in names
        assert "ras_uncorrectable_cold_reset" in names


class TestThermalPreset:
    def test_has_throttle_and_recovery(self):
        rules = thermal_throttle_rules()
        names = {r.name for r in rules}
        assert "thermal_throttle" in names
        assert "thermal_recovery" in names
        assert "thermal_critical_reset" in names

    def test_throttle_targets_thermal_domains(self):
        rules = {r.name: r for r in thermal_throttle_rules()}
        throttle = rules["thermal_throttle"]
        assert "core_thermal" in throttle.domains
        assert "memory_thermal" in throttle.domains


class TestPowerPreset:
    def test_has_throttle_and_critical(self):
        rules = power_management_rules()
        names = {r.name for r in rules}
        assert "power_throttle" in names
        assert "power_critical" in names


class TestLifecyclePreset:
    def test_has_add_and_remove(self):
        rules = device_lifecycle_rules()
        names = {r.name for r in rules}
        assert "device_added_log" in names
        assert "device_removed_log" in names


class TestGroupPreset:
    def test_has_group_rules(self):
        rules = group_policy_rules()
        names = {r.name for r in rules}
        assert "group_health_warning" in names
        assert "group_thermal_throttle" in names
        assert "group_ras_reset" in names


class TestAllDefaults:
    def test_includes_major_rule_sets(self):
        rules = all_default_rules()
        names = {r.name for r in rules}
        # Spot-check that major rule sets are present
        assert "pcode_error_log" in names
        assert "survivability_detected" in names
        assert "ras_correctable_log" in names
        assert "thermal_throttle" in names
        assert "power_throttle" in names
        assert "device_added_log" in names

    def test_no_duplicate_names(self):
        rules = all_default_rules()
        names = [r.name for r in rules]
        assert len(names) == len(set(names)), f"Duplicate names: {names}"
