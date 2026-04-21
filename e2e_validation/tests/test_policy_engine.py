#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Unit tests for the policy engine.

These tests verify policy rule matching, cooldown, max_fires, severity
filtering, domain filtering, and BDF scoping without requiring a live
gRPC connection or xpu-smi binary.
"""

import pytest

from e2e_validation.events.event_types import DeviceEvent, EventType, SeverityLevel
from e2e_validation.policies.engine import PolicyEngine, PolicyRule


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------


@pytest.fixture
def engine():
    return PolicyEngine()


def _noop_action(event, smi):
    pass


def _make_event(
    event_type=EventType.HEALTH_WARNING,
    bdf="0000:01:00.0",
    domain="core_thermal",
    severity=SeverityLevel.WARNING,
    **kwargs,
):
    return DeviceEvent(
        event_type=event_type,
        bdf=bdf,
        domain=domain,
        severity=severity,
        **kwargs,
    )


# ---------------------------------------------------------------------------
# Basic matching
# ---------------------------------------------------------------------------


class TestPolicyMatching:
    def test_event_type_match(self, engine):
        engine.add_rule(
            PolicyRule(
                name="test",
                event_types={EventType.HEALTH_WARNING},
                action=_noop_action,
            )
        )
        engine.handle_event(_make_event(event_type=EventType.HEALTH_WARNING))
        assert len(engine.firings) == 1

    def test_event_type_no_match(self, engine):
        engine.add_rule(
            PolicyRule(
                name="test",
                event_types={EventType.HEALTH_CRITICAL},
                action=_noop_action,
            )
        )
        engine.handle_event(_make_event(event_type=EventType.HEALTH_WARNING))
        assert len(engine.firings) == 0

    def test_min_severity(self, engine):
        engine.add_rule(
            PolicyRule(
                name="test",
                event_types={EventType.HEALTH_WARNING, EventType.HEALTH_CRITICAL},
                action=_noop_action,
                min_severity=SeverityLevel.CRITICAL,
            )
        )
        engine.handle_event(_make_event(severity=SeverityLevel.WARNING))
        assert len(engine.firings) == 0

        engine.handle_event(
            _make_event(
                severity=SeverityLevel.CRITICAL, event_type=EventType.HEALTH_CRITICAL
            )
        )
        assert len(engine.firings) == 1

    def test_domain_filter(self, engine):
        engine.add_rule(
            PolicyRule(
                name="test",
                event_types={EventType.HEALTH_WARNING},
                action=_noop_action,
                domains={"memory"},
            )
        )
        engine.handle_event(_make_event(domain="core_thermal"))
        assert len(engine.firings) == 0

        engine.handle_event(_make_event(domain="memory"))
        assert len(engine.firings) == 1

    def test_bdf_filter(self, engine):
        engine.add_rule(
            PolicyRule(
                name="test",
                event_types={EventType.HEALTH_WARNING},
                action=_noop_action,
                bdfs={"0000:02:00.0"},
            )
        )
        engine.handle_event(_make_event(bdf="0000:01:00.0"))
        assert len(engine.firings) == 0

        engine.handle_event(_make_event(bdf="0000:02:00.0"))
        assert len(engine.firings) == 1

    def test_disabled_rule(self, engine):
        engine.add_rule(
            PolicyRule(
                name="test",
                event_types={EventType.HEALTH_WARNING},
                action=_noop_action,
                enabled=False,
            )
        )
        engine.handle_event(_make_event())
        assert len(engine.firings) == 0


# ---------------------------------------------------------------------------
# Cooldown
# ---------------------------------------------------------------------------


class TestCooldown:
    def test_cooldown_suppresses(self, engine):
        engine.add_rule(
            PolicyRule(
                name="cd",
                event_types={EventType.HEALTH_WARNING},
                action=_noop_action,
                cooldown_s=10.0,
            )
        )
        engine.handle_event(_make_event())
        engine.handle_event(_make_event())
        assert len(engine.firings) == 1

    def test_different_bdf_not_suppressed(self, engine):
        engine.add_rule(
            PolicyRule(
                name="cd",
                event_types={EventType.HEALTH_WARNING},
                action=_noop_action,
                cooldown_s=10.0,
            )
        )
        engine.handle_event(_make_event(bdf="0000:01:00.0"))
        engine.handle_event(_make_event(bdf="0000:02:00.0"))
        assert len(engine.firings) == 2


# ---------------------------------------------------------------------------
# Max fires
# ---------------------------------------------------------------------------


class TestMaxFires:
    def test_max_fires_limit(self, engine):
        engine.add_rule(
            PolicyRule(
                name="mf",
                event_types={EventType.HEALTH_CRITICAL},
                action=_noop_action,
                max_fires=2,
            )
        )
        for i in range(5):
            engine.handle_event(
                _make_event(
                    bdf=f"0000:0{i}:00.0",
                    event_type=EventType.HEALTH_CRITICAL,
                    severity=SeverityLevel.CRITICAL,
                )
            )
        assert len(engine.firings) == 2

    def test_unlimited_fires(self, engine):
        engine.add_rule(
            PolicyRule(
                name="ul",
                event_types={EventType.HEALTH_WARNING},
                action=_noop_action,
                max_fires=0,
            )
        )
        for i in range(10):
            engine.handle_event(_make_event(bdf=f"0000:0{i}:00.0"))
        assert len(engine.firings) == 10


# ---------------------------------------------------------------------------
# Action errors
# ---------------------------------------------------------------------------


class TestActionErrors:
    def test_action_exception_recorded(self, engine):
        def bad_action(event, smi):
            raise RuntimeError("simulated failure")

        engine.add_rule(
            PolicyRule(
                name="err",
                event_types={EventType.HEALTH_WARNING},
                action=bad_action,
            )
        )
        engine.handle_event(_make_event())
        assert len(engine.firings) == 1
        assert not engine.firings[0].success
        assert "simulated failure" in engine.firings[0].error


# ---------------------------------------------------------------------------
# Rule removal
# ---------------------------------------------------------------------------


class TestRuleManagement:
    def test_remove_rule(self, engine):
        engine.add_rule(
            PolicyRule(
                name="to_remove",
                event_types={EventType.HEALTH_WARNING},
                action=_noop_action,
            )
        )
        engine.remove_rule("to_remove")
        engine.handle_event(_make_event())
        assert len(engine.firings) == 0

    def test_multiple_rules_fire(self, engine):
        engine.add_rule(
            PolicyRule(
                name="r1",
                event_types={EventType.HEALTH_WARNING},
                action=_noop_action,
            )
        )
        engine.add_rule(
            PolicyRule(
                name="r2",
                event_types={EventType.HEALTH_WARNING},
                action=_noop_action,
            )
        )
        engine.handle_event(_make_event())
        assert len(engine.firings) == 2
