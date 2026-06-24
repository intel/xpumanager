#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Device-event watcher.

Consumes the ``WatchDeviceEvents`` gRPC stream and translates each
``DeviceEventResponse`` into a :class:`DeviceEvent`.  This is the stream
that carries *discrete* hardware events — survivability, RAS, reset
required, thermal, etc. — as opposed to :class:`HealthWatcher`, which
consumes ``WatchDeviceHealth`` (periodic health-domain status).

The daemon sets each event's ``reason`` to the lowercased Level-Zero
sysman event-flag name (e.g. ``survivability_mode_detected``,
``ras_uncorrectable_errors``, ``device_reset_required``), so the mapping
below keys off those exact strings, with a substring fallback so a future
rename that keeps the salient word still classifies correctly.
"""

import logging
import threading
from collections.abc import Callable
from typing import Optional

from ..grpc_client import GrpcClient
from .. import deviceinfo_pb2 as pb2
from .event_types import DeviceEvent, EventType, SeverityLevel

log = logging.getLogger(__name__)

EventListener = Callable[[DeviceEvent], None]

# Maps proto event severity → our SeverityLevel enum.  The event stream
# uses OTel-aligned severities (EventSeverityLevel), so collapse them onto
# the coarser health SeverityLevel the rest of the tool speaks.
_PROTO_EVENT_SEV_MAP = {
    pb2.EVENT_SEVERITY_LEVEL_UNSPECIFIED: SeverityLevel.UNKNOWN,
    pb2.EVENT_SEVERITY_LEVEL_TRACE: SeverityLevel.OK,
    pb2.EVENT_SEVERITY_LEVEL_DEBUG: SeverityLevel.OK,
    pb2.EVENT_SEVERITY_LEVEL_INFO: SeverityLevel.OK,
    pb2.EVENT_SEVERITY_LEVEL_WARN: SeverityLevel.WARNING,
    pb2.EVENT_SEVERITY_LEVEL_ERROR: SeverityLevel.CRITICAL,
    pb2.EVENT_SEVERITY_LEVEL_FATAL: SeverityLevel.FAILED,
}

# Exact reason → EventType.  Reasons are the lowercased L0 sysman event
# flag names emitted by xpumd (see EVENT_TYPE_FLAG_* / events_translator.go).
_REASON_EVENT_MAP = {
    "survivability_mode_detected": EventType.DEVICE_SURVIVABILITY,
    "ras_correctable_errors": EventType.RAS_CORRECTABLE,
    "ras_uncorrectable_errors": EventType.RAS_UNCORRECTABLE,
    "device_reset_required": EventType.DEVICE_WEDGED,
    "freq_throttled": EventType.FREQUENCY_THROTTLE,
    "temp_critical": EventType.THERMAL_THROTTLE,
    "temp_threshold1": EventType.THERMAL_THROTTLE,
    "temp_threshold2": EventType.THERMAL_THROTTLE,
    "energy_threshold_crossed": EventType.POWER_THROTTLE,
    "device_attach": EventType.DEVICE_ADDED,
    "device_detach": EventType.DEVICE_REMOVED,
}

# Reasons that denote a generic health-domain transition: the event type is
# derived from the event's *severity* (see _SEV_HEALTH_EVENT) rather than the
# reason alone, so a CRITICAL/FATAL health event is not flattened to WARNING.
_HEALTH_DOMAIN_REASONS = frozenset(
    {"mem_health", "fabric_port_health", "pci_link_health"}
)

# Severity → health-degradation EventType, for the health-domain reasons above.
_SEV_HEALTH_EVENT = {
    SeverityLevel.OK: EventType.HEALTH_OK,
    SeverityLevel.UNKNOWN: EventType.HEALTH_WARNING,
    SeverityLevel.WARNING: EventType.HEALTH_WARNING,
    SeverityLevel.CRITICAL: EventType.HEALTH_CRITICAL,
    SeverityLevel.FAILED: EventType.HEALTH_FAILED,
}

# Substring fallback, checked in order when no exact reason matches.  Lets a
# reason rename that keeps the salient keyword still classify correctly.
# Order matters: more specific keywords come first so that a reason carrying
# two tokens (e.g. a hypothetical "thermal_reset") classifies by the more
# meaningful one rather than by the generic "reset"/"ras".  "uncorrectable"
# must precede "correctable" since it contains it as a substring.
_REASON_SUBSTR_MAP = (
    ("survivab", EventType.DEVICE_SURVIVABILITY),
    ("pcode", EventType.PCODE_ERROR),
    ("wedge", EventType.DEVICE_WEDGED),
    ("uncorrectable", EventType.RAS_UNCORRECTABLE),
    ("correctable", EventType.RAS_CORRECTABLE),
    ("ras", EventType.RAS_CORRECTABLE),
    ("thermal", EventType.THERMAL_THROTTLE),
    ("temp", EventType.THERMAL_THROTTLE),
    ("freq", EventType.FREQUENCY_THROTTLE),
    ("power", EventType.POWER_THROTTLE),
    ("energy", EventType.POWER_THROTTLE),
    ("reset", EventType.DEVICE_WEDGED),
)


def classify_event_reason(
    reason: str, severity: SeverityLevel = SeverityLevel.UNKNOWN
) -> Optional[EventType]:
    """Map a gRPC event ``reason`` string to an :class:`EventType`.

    *severity* is used only for the generic health-domain reasons
    (``mem_health`` etc.), whose specific event type (WARNING vs
    CRITICAL vs FAILED) depends on how bad the condition is.  Returns
    ``None`` if the reason does not correspond to any event type the
    validators care about.
    """
    key = (reason or "").strip().lower()
    if not key:
        return None
    if key in _HEALTH_DOMAIN_REASONS:
        return _SEV_HEALTH_EVENT.get(severity, EventType.HEALTH_WARNING)
    if key in _REASON_EVENT_MAP:
        return _REASON_EVENT_MAP[key]
    for needle, etype in _REASON_SUBSTR_MAP:
        if needle in key:
            return etype
    return None


class EventWatcher:
    """Watches the ``WatchDeviceEvents`` stream and emits ``DeviceEvent``s.

    Runs in a background thread, mirroring :class:`HealthWatcher`'s
    collection API (``events``, ``events_of_type``, ``start``/``stop``) so
    that callers can treat the two streams uniformly.
    """

    # A device in (say) survivability mode re-emits the same event on every
    # daemon poll, so the raw stream can deliver millions of *identical*
    # events in a single wait window.  We only need to know that an event of
    # a given (type, device, reason) was seen, so we record the first of each
    # distinct signature and suppress the rest — this bounds memory to the
    # number of distinct events regardless of how long the fault persists.
    _MAX_EVENTS = 10000  # hard backstop in case signatures are unexpectedly varied

    def __init__(self, client: GrpcClient) -> None:
        self._client = client
        self._listeners: list[EventListener] = []
        self._thread: Optional[threading.Thread] = None
        self._stop = threading.Event()
        self._events: list[DeviceEvent] = []
        self._seen: set[tuple] = set()
        self._suppressed = 0
        self._lock = threading.Lock()

    # ------------------------------------------------------------------
    # Listener / event access
    # ------------------------------------------------------------------

    def add_listener(self, listener: EventListener) -> None:
        self._listeners.append(listener)

    @staticmethod
    def _signature(event: DeviceEvent) -> tuple:
        # Severity is part of the signature so a severity *escalation* on an
        # otherwise-identical event (same type/device/reason) is recorded as
        # a distinct event rather than suppressed as a duplicate.
        return (event.event_type, event.bdf, event.reason, event.severity)

    def _emit(self, event: DeviceEvent) -> None:
        sig = self._signature(event)
        with self._lock:
            if sig in self._seen:
                # Duplicate of an event already recorded (the daemon re-emits
                # persistent faults on every poll); count it and drop it.
                self._suppressed += 1
                return
            if len(self._events) >= self._MAX_EVENTS:
                self._suppressed += 1
                return
            self._seen.add(sig)
            self._events.append(event)
        log.info("Event: %s", event)
        for listener in self._listeners:
            try:
                listener(event)
            except Exception:
                log.exception("Listener %r failed on %s", listener, event)

    @property
    def events(self) -> list[DeviceEvent]:
        with self._lock:
            return list(self._events)

    def events_of_type(self, *types: EventType) -> list[DeviceEvent]:
        with self._lock:
            return [e for e in self._events if e.event_type in types]

    def clear_events(self) -> None:
        with self._lock:
            self._events.clear()
            self._seen.clear()
            self._suppressed = 0

    @property
    def suppressed_count(self) -> int:
        """Number of duplicate/over-cap events dropped (diagnostic)."""
        with self._lock:
            return self._suppressed

    # ------------------------------------------------------------------
    # Response processing
    # ------------------------------------------------------------------

    @staticmethod
    def _bdf(resp: pb2.DeviceEventResponse) -> str:
        dev = resp.device
        if dev.HasField("pci") and dev.pci.bdf:
            return dev.pci.bdf
        return dev.uuid

    def _process_event(self, resp: pb2.DeviceEventResponse) -> None:
        severity = _PROTO_EVENT_SEV_MAP.get(resp.severity, SeverityLevel.UNKNOWN)
        etype = classify_event_reason(resp.reason, severity)
        if etype is None:
            # Not an event family the validators care about; record at
            # debug level but do not emit a DeviceEvent.
            log.debug("Ignoring unclassified device event: reason=%r", resp.reason)
            return
        self._emit(
            DeviceEvent(
                event_type=etype,
                bdf=self._bdf(resp),
                uuid=resp.device.uuid,
                severity=severity,
                reason=resp.reason,
                message=resp.message,
            )
        )

    # ------------------------------------------------------------------
    # Background thread
    # ------------------------------------------------------------------

    def start(self, timeout: Optional[float] = None) -> None:
        if self._thread is not None and self._thread.is_alive():
            raise RuntimeError("Event watcher already running")
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, args=(timeout,), daemon=True)
        self._thread.start()

    def stop(self, join_timeout: float = 5.0) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=join_timeout)

    def _run(self, timeout: Optional[float]) -> None:
        try:
            for resp in self._client.watch_device_events(timeout=timeout):
                if self._stop.is_set():
                    break
                self._process_event(resp)
        except Exception:
            if not self._stop.is_set():
                log.exception("Device event stream error")
