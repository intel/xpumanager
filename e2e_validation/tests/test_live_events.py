#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Unit tests for event-driven live validators.

These validators must:
  * PASS only when a *real* matching event is observed on the stream;
  * FAIL (not PASS, not SKIP) when no qualifying event arrives within the
    timeout window;
  * never synthesize events themselves.

The tests drive the shared :class:`LiveStreamSession` directly, injecting
events into its watcher, so no live gRPC server is required.
"""

import time

import pytest

from e2e_validation.events.event_types import DeviceEvent, EventType, SeverityLevel
from e2e_validation.events.event_watcher import classify_event_reason
from e2e_validation.validators.base import ValidationStatus
from e2e_validation.validators.live_events import (
    LiveStreamSession,
    LiveHealthEventValidator,
    LivePcodeErrorValidator,
    LiveRASValidator,
    LiveSurvivabilityValidator,
    LiveThermalThrottleValidator,
)


class _FakeClient:
    """Stub satisfying LiveStreamSession/HealthWatcher construction."""

    pass


def _session(events=(), timeout=0.05, *, on_events=False):
    """Build a session whose watcher already holds *events*.

    We arm the deadline manually instead of calling ``start()`` so the
    test never spawns a real gRPC stream.  By default the events are
    injected into the health watcher; pass ``on_events=True`` to inject
    them into the device-events watcher instead (the ``WatchDeviceEvents``
    stream), which is where real fault events actually arrive.
    """
    session = LiveStreamSession(_FakeClient(), timeout_s=timeout)
    session._deadline = time.monotonic() + timeout
    watcher = session._event_watcher if on_events else session._watcher
    for ev in events:
        watcher._emit(ev)
    return session


def _pcode_event(bdf="0000:03:00.0"):
    return DeviceEvent(
        event_type=EventType.PCODE_ERROR,
        bdf=bdf,
        domain="gpu_core",
        severity=SeverityLevel.CRITICAL,
        reason="pcode_timeout",
    )


class TestFailsWhenNoEvent:
    def test_pcode_fails_on_timeout(self):
        v = LivePcodeErrorValidator(_session(timeout=0.05))
        result = v.run()
        assert result.status == ValidationStatus.FAIL
        assert "pcode_error" in result.message

    def test_ras_fails_on_timeout(self):
        v = LiveRASValidator(_session(timeout=0.05))
        assert v.run().status == ValidationStatus.FAIL

    def test_survivability_fails_on_timeout(self):
        v = LiveSurvivabilityValidator(_session(timeout=0.05))
        assert v.run().status == ValidationStatus.FAIL

    def test_thermal_fails_on_timeout(self):
        v = LiveThermalThrottleValidator(_session(timeout=0.05))
        assert v.run().status == ValidationStatus.FAIL

    def test_health_fails_on_timeout(self):
        v = LiveHealthEventValidator(_session(timeout=0.05))
        assert v.run().status == ValidationStatus.FAIL


class TestPassesOnRealEvent:
    def test_pcode_passes_when_event_present(self):
        session = _session(events=[_pcode_event()])
        result = LivePcodeErrorValidator(session).run()
        assert result.status == ValidationStatus.PASS
        assert "0000:03:00.0" in result.message

    def test_health_passes_on_warning(self):
        ev = DeviceEvent(
            event_type=EventType.HEALTH_WARNING,
            bdf="0000:01:00.0",
            domain="memory",
            severity=SeverityLevel.WARNING,
        )
        result = LiveHealthEventValidator(_session(events=[ev])).run()
        assert result.status == ValidationStatus.PASS


class TestDoesNotMatchWrongEvent:
    def test_pcode_ignores_unrelated_event(self):
        # A RAS event must not satisfy the pcode validator.
        ras = DeviceEvent(
            event_type=EventType.RAS_CORRECTABLE,
            bdf="0000:03:00.0",
            domain="ras_cache",
            severity=SeverityLevel.WARNING,
        )
        result = LivePcodeErrorValidator(_session(events=[ras], timeout=0.05)).run()
        assert result.status == ValidationStatus.FAIL


class TestNeverSynthesizes:
    def test_no_events_are_created_by_validator(self):
        # After running against empty streams, neither watcher must hold any
        # events — the validator did not fabricate any.
        session = _session(timeout=0.05)
        LivePcodeErrorValidator(session).run()
        assert session.watcher.events == []
        assert session.event_watcher.events == []


class TestConsumesDeviceEventsStream:
    """Real fault events arrive on WatchDeviceEvents, not WatchDeviceHealth."""

    def test_survivability_passes_from_events_stream(self):
        # A survivability event delivered on the device-events stream (where
        # the daemon actually emits it) must satisfy live_survivability.
        ev = DeviceEvent(
            event_type=EventType.DEVICE_SURVIVABILITY,
            bdf="0000:03:00.0",
            severity=SeverityLevel.CRITICAL,
            reason="survivability_mode_detected",
        )
        result = LiveSurvivabilityValidator(
            _session(events=[ev], on_events=True)
        ).run()
        assert result.status == ValidationStatus.PASS
        assert "0000:03:00.0" in result.message

    def test_ras_passes_from_events_stream(self):
        ev = DeviceEvent(
            event_type=EventType.RAS_UNCORRECTABLE,
            bdf="0000:03:00.0",
            severity=SeverityLevel.CRITICAL,
            reason="ras_uncorrectable_errors",
        )
        result = LiveRASValidator(_session(events=[ev], on_events=True)).run()
        assert result.status == ValidationStatus.PASS

    def test_wait_for_searches_both_streams(self):
        # Health event on the health stream + RAS event on the events stream;
        # each validator finds its match regardless of which stream carried it.
        health_ev = DeviceEvent(
            event_type=EventType.HEALTH_WARNING,
            bdf="0000:03:00.0",
            domain="gpu",
            severity=SeverityLevel.WARNING,
        )
        ras_ev = DeviceEvent(
            event_type=EventType.RAS_CORRECTABLE,
            bdf="0000:03:00.0",
            severity=SeverityLevel.WARNING,
            reason="ras_correctable_errors",
        )
        session = LiveStreamSession(_FakeClient(), timeout_s=0.05)
        session._deadline = time.monotonic() + 0.05
        session._watcher._emit(health_ev)
        session._event_watcher._emit(ras_ev)
        assert LiveHealthEventValidator(session).run().status == ValidationStatus.PASS
        assert LiveRASValidator(session).run().status == ValidationStatus.PASS


class TestEventDedup:
    """A persistent fault re-emits the same event on every daemon poll."""

    def test_duplicate_events_are_suppressed(self):
        from e2e_validation.events.event_watcher import EventWatcher

        ew = EventWatcher(_FakeClient())
        ev = DeviceEvent(
            event_type=EventType.DEVICE_SURVIVABILITY,
            bdf="0000:03:00.0",
            severity=SeverityLevel.CRITICAL,
            reason="survivability_mode_detected",
        )
        for _ in range(1000):
            ew._emit(ev)
        # Only the first distinct (type, bdf, reason) is retained; the rest
        # are suppressed so memory does not blow up on a sustained fault.
        assert len(ew.events) == 1
        assert ew.suppressed_count == 999

    def test_distinct_events_are_all_kept(self):
        from e2e_validation.events.event_watcher import EventWatcher

        ew = EventWatcher(_FakeClient())
        for reason, etype in (
            ("survivability_mode_detected", EventType.DEVICE_SURVIVABILITY),
            ("ras_uncorrectable_errors", EventType.RAS_UNCORRECTABLE),
        ):
            ew._emit(
                DeviceEvent(event_type=etype, bdf="0000:03:00.0", reason=reason)
            )
        assert len(ew.events) == 2
        assert ew.suppressed_count == 0


class TestEventReasonClassification:
    """The daemon sets reason = lowercased L0 sysman event-flag name."""

    @pytest.mark.parametrize(
        "reason, expected",
        [
            ("survivability_mode_detected", EventType.DEVICE_SURVIVABILITY),
            ("ras_correctable_errors", EventType.RAS_CORRECTABLE),
            ("ras_uncorrectable_errors", EventType.RAS_UNCORRECTABLE),
            ("device_reset_required", EventType.DEVICE_WEDGED),
            ("temp_critical", EventType.THERMAL_THROTTLE),
            ("freq_throttled", EventType.FREQUENCY_THROTTLE),
            ("SURVIVABILITY_MODE_DETECTED", EventType.DEVICE_SURVIVABILITY),
        ],
    )
    def test_known_reasons_map(self, reason, expected):
        assert classify_event_reason(reason) == expected

    @pytest.mark.parametrize("reason", ["", "device_attach_not_a_fault_xyz", "unknown"])
    def test_unrelated_reasons_are_unmapped_or_benign(self, reason):
        # Empty / unknown reasons must not be classified as a fault family.
        result = classify_event_reason(reason)
        assert result not in (
            EventType.DEVICE_SURVIVABILITY,
            EventType.RAS_UNCORRECTABLE,
            EventType.PCODE_ERROR,
        )

    @pytest.mark.parametrize(
        "severity, expected",
        [
            (SeverityLevel.WARNING, EventType.HEALTH_WARNING),
            (SeverityLevel.CRITICAL, EventType.HEALTH_CRITICAL),
            (SeverityLevel.FAILED, EventType.HEALTH_FAILED),
        ],
    )
    def test_health_domain_reason_is_severity_aware(self, severity, expected):
        # A generic health-domain event must take its specific type from the
        # severity, not be flattened to WARNING.
        assert classify_event_reason("mem_health", severity) == expected

    def test_specific_keyword_beats_generic_reset(self):
        # A reason carrying both a specific token and the generic "reset"
        # must classify by the specific one.
        assert classify_event_reason("thermal_reset") == EventType.THERMAL_THROTTLE


class TestSeverityEscalationNotSuppressed:
    def test_escalation_is_recorded_as_distinct_event(self):
        from e2e_validation.events.event_watcher import EventWatcher

        ew = EventWatcher(_FakeClient())
        warn = DeviceEvent(
            event_type=EventType.HEALTH_WARNING,
            bdf="0000:03:00.0",
            severity=SeverityLevel.WARNING,
            reason="mem_health",
        )
        crit = DeviceEvent(
            event_type=EventType.HEALTH_CRITICAL,
            bdf="0000:03:00.0",
            severity=SeverityLevel.CRITICAL,
            reason="mem_health",
        )
        ew._emit(warn)
        ew._emit(warn)  # duplicate of the warning -> suppressed
        ew._emit(crit)  # escalation -> distinct, kept
        assert len(ew.events) == 2
        assert ew.suppressed_count == 1


class TestSharedDeadline:
    def test_second_validator_returns_immediately(self):
        # Two validators share one window: the first consumes the wait, the
        # second evaluates instantly against already-collected events.
        session = _session(events=[_pcode_event()], timeout=0.05)
        t0 = time.monotonic()
        LivePcodeErrorValidator(session).run()
        LiveRASValidator(session).run()  # no RAS event -> FAIL, but fast
        elapsed = time.monotonic() - t0
        # Far less than 2x the per-validator timeout would be if serial waits.
        assert elapsed < 0.5


class TestTimeoutValidation:
    @pytest.mark.parametrize("bad", [0, -1, -0.5])
    def test_non_positive_timeout_rejected(self, bad):
        # A non-positive window would make every event-driven validator FAIL
        # instantly; reject it at construction time instead.
        with pytest.raises(ValueError):
            LiveStreamSession(_FakeClient(), timeout_s=bad)


class TestDoesNotOvershootDeadline:
    def test_wait_returns_at_deadline(self):
        # With no matching event, wait_for must return shortly after the
        # deadline, not a full poll interval past it.
        session = _session(timeout=0.05)
        t0 = time.monotonic()
        assert session.wait_for(EventType.PCODE_ERROR) is None
        elapsed = time.monotonic() - t0
        # deadline (0.05) + at most one clamped sleep; comfortably under the
        # 0.25s default poll interval added on top.
        assert elapsed < 0.2
