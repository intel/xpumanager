#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Unit tests for the health watcher event emission logic.

Tests process_response directly with synthetic protobuf messages to
verify correct event classification without needing a live gRPC server.
"""

import pytest

from e2e_validation import deviceinfo_pb2 as pb2
from e2e_validation.events.event_types import EventType
from e2e_validation.events.health_watcher import HealthWatcher


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


class FakeGrpcClient:
    """Stub that satisfies the HealthWatcher constructor."""

    pass


def _make_device(
    uuid="test-uuid",
    model="Test GPU",
    bdf="0000:01:00.0",
    health=None,
):
    dev = pb2.DeviceHealth()
    dev.info.uuid = uuid
    dev.info.model = model
    dev.info.pci.bdf = bdf
    if health:
        for h in health:
            hs = dev.health.add()
            hs.name = h["name"]
            hs.severity = h["severity"]
            hs.reason = h.get("reason", "")
            hs.message = h.get("message", "")
    return dev


def _make_response(*devices):
    resp = pb2.DeviceHealthResponse()
    for d in devices:
        resp.devices.append(d)
    return resp


@pytest.fixture
def watcher():
    return HealthWatcher(FakeGrpcClient())


# ---------------------------------------------------------------------------
# Device discovery
# ---------------------------------------------------------------------------


class TestDeviceDiscovery:
    def test_initial_snapshot_emits_discovered(self, watcher):
        dev = _make_device(bdf="0000:01:00.0")
        watcher._process_response(_make_response(dev), first=True)

        events = watcher.events_of_type(EventType.DEVICE_DISCOVERED)
        assert len(events) == 1
        assert events[0].bdf == "0000:01:00.0"

    def test_subsequent_new_device_emits_added(self, watcher):
        dev1 = _make_device(bdf="0000:01:00.0", uuid="a")
        watcher._process_response(_make_response(dev1), first=True)

        dev2 = _make_device(bdf="0000:02:00.0", uuid="b")
        watcher._process_response(_make_response(dev1, dev2), first=False)

        added = watcher.events_of_type(EventType.DEVICE_ADDED)
        assert len(added) == 1
        assert added[0].bdf == "0000:02:00.0"


# ---------------------------------------------------------------------------
# Device removal
# ---------------------------------------------------------------------------


class TestDeviceRemoval:
    def test_device_removal_emits_removed(self, watcher):
        dev1 = _make_device(bdf="0000:01:00.0", uuid="a")
        dev2 = _make_device(bdf="0000:02:00.0", uuid="b")
        watcher._process_response(_make_response(dev1, dev2), first=True)

        watcher._process_response(_make_response(dev1), first=False)

        removed = watcher.events_of_type(EventType.DEVICE_REMOVED)
        assert len(removed) == 1
        assert removed[0].bdf == "0000:02:00.0"


# ---------------------------------------------------------------------------
# Health events
# ---------------------------------------------------------------------------


class TestHealthEvents:
    def test_unhealthy_emits_warning(self, watcher):
        dev = _make_device(
            health=[{"name": "memory", "severity": pb2.SEVERITY_LEVEL_WARNING}],
        )
        watcher._process_response(_make_response(dev), first=True)

        warnings = watcher.events_of_type(EventType.HEALTH_WARNING)
        assert len(warnings) == 1
        assert warnings[0].domain == "memory"

    def test_same_severity_no_duplicate(self, watcher):
        dev = _make_device(
            health=[{"name": "memory", "severity": pb2.SEVERITY_LEVEL_WARNING}],
        )
        watcher._process_response(_make_response(dev), first=True)
        watcher._process_response(_make_response(dev), first=False)

        warnings = watcher.events_of_type(EventType.HEALTH_WARNING)
        assert len(warnings) == 1

    def test_severity_escalation(self, watcher):
        dev_warn = _make_device(
            health=[{"name": "memory", "severity": pb2.SEVERITY_LEVEL_WARNING}],
        )
        dev_crit = _make_device(
            health=[{"name": "memory", "severity": pb2.SEVERITY_LEVEL_CRITICAL}],
        )

        watcher._process_response(_make_response(dev_warn), first=True)
        watcher._process_response(_make_response(dev_crit), first=False)

        all_health = watcher.events_of_type(
            EventType.HEALTH_WARNING, EventType.HEALTH_CRITICAL
        )
        assert len(all_health) == 2

    def test_recovery_emits_event(self, watcher):
        dev_warn = _make_device(
            health=[{"name": "memory", "severity": pb2.SEVERITY_LEVEL_WARNING}],
        )
        dev_ok = _make_device(
            health=[{"name": "memory", "severity": pb2.SEVERITY_LEVEL_OK}],
        )

        watcher._process_response(_make_response(dev_warn), first=True)
        watcher._process_response(_make_response(dev_ok), first=False)

        recovered = watcher.events_of_type(EventType.HEALTH_RECOVERED)
        assert len(recovered) == 1
        assert recovered[0].domain == "memory"

    def test_domain_removal_triggers_recovery(self, watcher):
        dev_warn = _make_device(
            health=[{"name": "memory", "severity": pb2.SEVERITY_LEVEL_WARNING}],
        )
        dev_no_health = _make_device(health=[])

        watcher._process_response(_make_response(dev_warn), first=True)
        watcher._process_response(_make_response(dev_no_health), first=False)

        recovered = watcher.events_of_type(EventType.HEALTH_RECOVERED)
        assert len(recovered) == 1

    def test_device_removal_clears_unhealthy(self, watcher):
        dev = _make_device(
            health=[{"name": "memory", "severity": pb2.SEVERITY_LEVEL_CRITICAL}],
        )
        watcher._process_response(_make_response(dev), first=True)
        assert ("0000:01:00.0", "memory") in watcher._unhealthy

        watcher._process_response(_make_response(), first=False)
        assert ("0000:01:00.0", "memory") not in watcher._unhealthy


# ---------------------------------------------------------------------------
# Event classification
# ---------------------------------------------------------------------------


class TestEventClassification:
    def test_pcode_error_classified(self, watcher):
        dev = _make_device(
            health=[
                {
                    "name": "gpu_core",
                    "severity": pb2.SEVERITY_LEVEL_CRITICAL,
                    "reason": "pcode_timeout",
                    "message": "Pcode firmware not responding",
                }
            ],
        )
        watcher._process_response(_make_response(dev), first=True)

        pcode = watcher.events_of_type(EventType.PCODE_ERROR)
        assert len(pcode) == 1

    def test_device_wedged_classified(self, watcher):
        dev = _make_device(
            health=[
                {
                    "name": "gpu_core",
                    "severity": pb2.SEVERITY_LEVEL_FAILED,
                    "reason": "device_wedged",
                    "message": "GPU compute engine unresponsive",
                }
            ],
        )
        watcher._process_response(_make_response(dev), first=True)

        wedged = watcher.events_of_type(EventType.DEVICE_WEDGED)
        assert len(wedged) == 1

    def test_survivability_classified(self, watcher):
        dev = _make_device(
            health=[
                {
                    "name": "firmware",
                    "severity": pb2.SEVERITY_LEVEL_CRITICAL,
                    "reason": "survivability_mode",
                    "message": "Device in survivability mode",
                }
            ],
        )
        watcher._process_response(_make_response(dev), first=True)

        surv = watcher.events_of_type(EventType.DEVICE_SURVIVABILITY)
        assert len(surv) == 1

    def test_ras_correctable_classified(self, watcher):
        dev = _make_device(
            health=[
                {
                    "name": "ras_cache",
                    "severity": pb2.SEVERITY_LEVEL_WARNING,
                    "reason": "ecc_correctable",
                    "message": "Correctable ECC error",
                }
            ],
        )
        watcher._process_response(_make_response(dev), first=True)

        ras = watcher.events_of_type(EventType.RAS_CORRECTABLE)
        assert len(ras) == 1

    def test_ras_uncorrectable_classified(self, watcher):
        dev = _make_device(
            health=[
                {
                    "name": "ras_memory",
                    "severity": pb2.SEVERITY_LEVEL_CRITICAL,
                    "reason": "uncorrectable_ecc",
                    "message": "Uncorrectable memory error",
                }
            ],
        )
        watcher._process_response(_make_response(dev), first=True)

        ras = watcher.events_of_type(EventType.RAS_UNCORRECTABLE)
        assert len(ras) == 1

    def test_thermal_throttle_classified(self, watcher):
        dev = _make_device(
            health=[
                {
                    "name": "core_thermal",
                    "severity": pb2.SEVERITY_LEVEL_WARNING,
                    "reason": "thermal_throttle",
                    "message": "Thermal throttle active",
                }
            ],
        )
        watcher._process_response(_make_response(dev), first=True)

        throttle = watcher.events_of_type(EventType.THERMAL_THROTTLE)
        assert len(throttle) == 1
