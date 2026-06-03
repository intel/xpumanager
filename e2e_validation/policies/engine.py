#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Policy engine.

A policy is a rule that maps an event (or pattern of events) to a
remediation action.  The engine consumes ``DeviceEvent`` objects from the
health watcher and evaluates each registered policy.

Architecture::

    HealthWatcher  ──events──►  PolicyEngine  ──triggers──►  Action
                                    │
                                    └── PolicyRule[]

Actions are pluggable callables that receive the triggering event and
the xpu-smi wrapper.  Built-in actions include cold-reset, firmware
update, frequency throttle, and notification logging.
"""

import logging
import time
from collections.abc import Callable
from dataclasses import dataclass, field
from typing import Optional

from ..events.event_types import DeviceEvent, EventType, SeverityLevel
from ..xpu_smi import XpuSmi

log = logging.getLogger(__name__)

# Type alias for action callables.
#
# An action returns ``True`` (or ``None`` for legacy log-only actions) when
# the operation succeeded, and ``False`` when it ran but the underlying
# operation failed.  Actions may also raise to signal failure; both a
# ``False`` return and a raised exception are recorded as a failed firing.
ActionFn = Callable[[DeviceEvent, Optional[XpuSmi]], Optional[bool]]


@dataclass
class PolicyRule:
    """A single event → action mapping.

    Attributes:
        name:           Human-readable rule name.
        event_types:    Set of event types this rule matches.
        min_severity:   Minimum severity required (inclusive).
        domains:        If non-empty, only match these health domains.
        bdfs:           If non-empty, only match these BDFs.
        action:         Callable to execute when the rule fires.
        cooldown_s:     Minimum seconds between consecutive firings for
                        the same (bdf, domain) pair.
        max_fires:      Maximum total number of times this rule may fire
                        (0 = unlimited).
        enabled:        Whether the rule is currently active.
    """

    name: str
    event_types: set[EventType]
    action: ActionFn
    min_severity: SeverityLevel = SeverityLevel.UNKNOWN
    domains: set[str] = field(default_factory=set)
    bdfs: set[str] = field(default_factory=set)
    cooldown_s: float = 0.0
    max_fires: int = 0
    enabled: bool = True


@dataclass
class PolicyFiring:
    """Record of a policy rule that was triggered."""

    rule_name: str
    event: DeviceEvent
    timestamp: float = field(default_factory=time.time)
    success: bool = True
    error: str = ""


class PolicyEngine:
    """Evaluates events against registered policy rules."""

    def __init__(self, xpu_smi: Optional[XpuSmi] = None) -> None:
        self._rules: list[PolicyRule] = []
        self._smi = xpu_smi
        self._firings: list[PolicyFiring] = []
        # (rule_name, bdf, domain) -> last fire time (for cooldown)
        self._last_fire: dict[tuple[str, str, str], float] = {}
        # rule_name -> total fire count
        self._fire_count: dict[str, int] = {}

    def add_rule(self, rule: PolicyRule) -> None:
        self._rules.append(rule)
        log.info(
            "Policy rule added: %s (events=%s)",
            rule.name,
            {e.value for e in rule.event_types},
        )

    def remove_rule(self, name: str) -> None:
        self._rules = [r for r in self._rules if r.name != name]

    @property
    def firings(self) -> list[PolicyFiring]:
        return list(self._firings)

    def handle_event(self, event: DeviceEvent) -> None:
        """Evaluate all rules against a single event."""
        for rule in self._rules:
            if not rule.enabled:
                continue
            if not self._matches(rule, event):
                continue
            self._fire(rule, event)

    def _matches(self, rule: PolicyRule, event: DeviceEvent) -> bool:
        if event.event_type not in rule.event_types:
            return False
        if event.severity < rule.min_severity:
            return False
        if rule.domains and event.domain not in rule.domains:
            return False
        if rule.bdfs and event.bdf not in rule.bdfs:
            return False

        # Cooldown check
        if rule.cooldown_s > 0:
            key = (rule.name, event.bdf, event.domain)
            last = self._last_fire.get(key, 0.0)
            if time.time() - last < rule.cooldown_s:
                return False

        # Max fires check
        if rule.max_fires > 0:
            if self._fire_count.get(rule.name, 0) >= rule.max_fires:
                return False

        return True

    def _fire(self, rule: PolicyRule, event: DeviceEvent) -> None:
        log.info("Policy %r triggered by %s", rule.name, event)
        key = (rule.name, event.bdf, event.domain)
        self._last_fire[key] = time.time()
        self._fire_count[rule.name] = self._fire_count.get(rule.name, 0) + 1

        firing = PolicyFiring(rule_name=rule.name, event=event)
        try:
            outcome = rule.action(event, self._smi)
            # Actions may return False to report a non-exceptional failure
            # (e.g. xpu-smi returned a non-zero rc). None means "log-only,
            # nothing to fail".
            if outcome is False:
                firing.success = False
                firing.error = (
                    f"action for rule {rule.name!r} reported failure"
                )
        except Exception as exc:
            log.exception("Action for rule %r failed", rule.name)
            firing.success = False
            firing.error = str(exc)
        self._firings.append(firing)
