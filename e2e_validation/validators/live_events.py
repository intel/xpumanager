#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Event-driven live validators.

Unlike the use-case validators in :mod:`validators.use_cases`, these
validators **never synthesize events**.  They attach to the live gRPC
streams and wait for *real* events that originate in the kernel (KMD)
and propagate

    KMD → L0 sysman → xpumd → gRPC socket → e2e_validation

The daemon exposes two streams and the validators consume **both**:

  * ``WatchDeviceHealth`` (via :class:`HealthWatcher`) — periodic
    health-domain status; the source of health-degradation transitions
    (WARNING/CRITICAL/FAILED).
  * ``WatchDeviceEvents`` (via :class:`EventWatcher`) — discrete hardware
    events: survivability, RAS, device-reset-required (wedge), thermal,
    etc.  This is the stream that carries the fault events most live
    validators target, so consuming only the health stream would make
    them FAIL even when a real fault occurred.

If no qualifying event arrives within the configured window the
validator reports **FAIL** (not PASS, not SKIP) — the whole point of
``--live`` is to prove the real event path works end to end.

All event-driven validators share a single :class:`LiveStreamSession`
so that one pair of background streams feeds every validator and the
wall-clock cost is bounded by a single ``live_event_timeout_s`` window
rather than ``N × timeout``.  Whichever validator runs first blocks until
the shared deadline; the rest then evaluate instantly against the events
collected during that window.
"""

import logging
import time
from typing import Optional

from .base import BaseValidator, ValidationResult, ValidationStatus
from ..events.event_types import DeviceEvent, EventType
from ..events.event_watcher import EventWatcher
from ..events.health_watcher import HealthWatcher
from ..grpc_client import GrpcClient

log = logging.getLogger(__name__)


class LiveStreamSession:
    """Owns the background stream watchers shared by event-driven validators.

    The session starts a :class:`HealthWatcher` (``WatchDeviceHealth``) and
    an :class:`EventWatcher` (``WatchDeviceEvents``) against the live gRPC
    socket and exposes a *shared deadline*: every validator waits until the
    same point in time, so the total run is bounded by one ``timeout_s``
    window regardless of how many event validators run.  ``wait_for``
    searches the events collected by both watchers.
    """

    def __init__(self, client: GrpcClient, timeout_s: float) -> None:
        if timeout_s <= 0:
            raise ValueError(
                f"live event timeout_s must be > 0 (got {timeout_s}); a "
                "non-positive window would make every event-driven validator "
                "FAIL instantly"
            )
        self._client = client
        self._watcher = HealthWatcher(client)
        self._event_watcher = EventWatcher(client)
        self._timeout_s = timeout_s
        self._deadline: Optional[float] = None
        self._poll_interval_s = 0.25

    @property
    def timeout_s(self) -> float:
        return self._timeout_s

    def start(self) -> None:
        """Begin streaming in the background and arm the shared deadline."""
        # Stream a little longer than the wait window so the watchers do
        # not tear the streams down before the last validator evaluates.
        self._watcher.start(timeout=self._timeout_s + 30.0)
        self._event_watcher.start(timeout=self._timeout_s + 30.0)
        self._deadline = time.monotonic() + self._timeout_s
        log.info(
            "Live stream session started; waiting up to %.0fs for real events",
            self._timeout_s,
        )

    def stop(self) -> None:
        self._watcher.stop()
        self._event_watcher.stop()

    @property
    def watcher(self) -> HealthWatcher:
        return self._watcher

    @property
    def event_watcher(self) -> EventWatcher:
        return self._event_watcher

    def wait_for(
        self,
        *event_types: EventType,
        bdf: Optional[str] = None,
        domain: Optional[str] = None,
    ) -> Optional[DeviceEvent]:
        """Block until a matching real event is seen or the deadline passes.

        Returns the first matching :class:`DeviceEvent`, or ``None`` if the
        shared deadline elapsed without one.  Events already collected
        before this call are considered, so validators that run after the
        deadline return immediately.  Both the health stream and the
        device-events stream are searched.
        """
        deadline = self._deadline if self._deadline is not None else time.monotonic()

        def _match() -> Optional[DeviceEvent]:
            for watcher in (self._watcher, self._event_watcher):
                for ev in watcher.events_of_type(*event_types):
                    if bdf is not None and ev.bdf != bdf:
                        continue
                    if domain is not None and ev.domain != domain:
                        continue
                    return ev
            return None

        # Fast path: already observed.
        found = _match()
        if found is not None:
            return found

        # Poll until the shared deadline, never sleeping past it: a short
        # --live-timeout must not be overshot by a full poll interval.
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            time.sleep(min(self._poll_interval_s, remaining))
            found = _match()
            if found is not None:
                return found
        return _match()


class EventDrivenValidator(BaseValidator):
    """Base for validators that wait for one real event family.

    Subclasses set ``name``, ``event_types`` and ``description``.
    """

    event_types: tuple[EventType, ...] = ()
    description: str = ""

    def __init__(self, session: LiveStreamSession) -> None:
        self._session = session

    def _run(self) -> ValidationResult:
        event = self._session.wait_for(*self.event_types)
        wanted = ", ".join(et.value for et in self.event_types)

        if event is not None:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.PASS,
                message=(
                    f"Observed real [{event.event_type.value}] on {event.bdf} "
                    f"via the live gRPC stream"
                ),
                details={
                    "observed_event": str(event),
                    "expected_event_types": list(
                        et.value for et in self.event_types
                    ),
                },
            )

        return ValidationResult(
            name=self.name,
            status=ValidationStatus.FAIL,
            message=(
                f"No {wanted} event received from the gRPC stream within "
                f"{self._session.timeout_s:.0f}s. {self.description}"
            ),
            details={
                "expected_event_types": list(et.value for et in self.event_types),
                "timeout_s": self._session.timeout_s,
                "hint": (
                    "On healthy hardware this event does not occur "
                    "spontaneously; it requires a real fault (or fault "
                    "injection) somewhere in KMD → sysman → xpumd."
                ),
            },
        )


class LiveHealthEventValidator(EventDrivenValidator):
    """Wait for any real unhealthy transition (WARNING/CRITICAL/FAILED)."""

    name = "live_health_event"
    event_types = (
        EventType.HEALTH_WARNING,
        EventType.HEALTH_CRITICAL,
        EventType.HEALTH_FAILED,
    )
    description = "Expected a real device health-degradation event."


class LivePcodeErrorValidator(EventDrivenValidator):
    """Wait for a real Pcode error / device-wedged event."""

    name = "live_pcode_error"
    event_types = (EventType.PCODE_ERROR, EventType.DEVICE_WEDGED)
    description = "Expected a real Pcode/device-wedged fault from the KMD."


class LiveRASValidator(EventDrivenValidator):
    """Wait for a real RAS (correctable or uncorrectable) event."""

    name = "live_ras_events"
    event_types = (EventType.RAS_CORRECTABLE, EventType.RAS_UNCORRECTABLE)
    description = "Expected a real RAS ECC event from the KMD."


class LiveSurvivabilityValidator(EventDrivenValidator):
    """Wait for a real device-survivability event."""

    name = "live_survivability"
    event_types = (EventType.DEVICE_SURVIVABILITY,)
    description = "Expected a real survivability-mode event from the KMD."


class LiveThermalThrottleValidator(EventDrivenValidator):
    """Wait for a real thermal-throttle event."""

    name = "live_thermal_throttle"
    event_types = (EventType.THERMAL_THROTTLE,)
    description = "Expected a real thermal-throttle event from the KMD."
