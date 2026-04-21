#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Health watcher daemon.

Consumes the WatchDeviceHealth gRPC stream and translates proto messages
into ``DeviceEvent`` objects.  Events are dispatched to registered
listeners (policies, loggers, validators).

This is the core "daemon" component of the E2E validation tool,
analogous to the ``XpuInfoWatcher`` in the xpuinfo-py PR but designed
for automated testing rather than interactive use.
"""

import logging
import threading
import time
from collections.abc import Callable
from typing import Optional

from ..grpc_client import GrpcClient
from .. import deviceinfo_pb2 as pb2
from .event_types import DeviceEvent, EventType, SeverityLevel

log = logging.getLogger(__name__)

# Maps proto severity → our SeverityLevel enum
_PROTO_SEV_MAP = {
    pb2.SEVERITY_LEVEL_UNKNOWN: SeverityLevel.UNKNOWN,
    pb2.SEVERITY_LEVEL_OK: SeverityLevel.OK,
    pb2.SEVERITY_LEVEL_WARNING: SeverityLevel.WARNING,
    pb2.SEVERITY_LEVEL_CRITICAL: SeverityLevel.CRITICAL,
    pb2.SEVERITY_LEVEL_FAILED: SeverityLevel.FAILED,
}

_UNHEALTHY = {SeverityLevel.WARNING, SeverityLevel.CRITICAL, SeverityLevel.FAILED}

# Maps severity → event type for health changes
_SEV_EVENT = {
    SeverityLevel.OK: EventType.HEALTH_OK,
    SeverityLevel.WARNING: EventType.HEALTH_WARNING,
    SeverityLevel.CRITICAL: EventType.HEALTH_CRITICAL,
    SeverityLevel.FAILED: EventType.HEALTH_FAILED,
}

EventListener = Callable[[DeviceEvent], None]


class HealthWatcher:
    """Watches the gRPC health stream and emits ``DeviceEvent`` objects.

    The watcher runs in a background thread so validation test code can
    continue executing while events are collected.
    """

    def __init__(self, client: GrpcClient) -> None:
        self._client = client
        self._listeners: list[EventListener] = []
        self._known: dict[str, pb2.DeviceHealth] = {}
        self._unhealthy: dict[tuple[str, str], SeverityLevel] = {}
        self._thread: Optional[threading.Thread] = None
        self._stop = threading.Event()
        self._events: list[DeviceEvent] = []
        self._lock = threading.Lock()

    # ------------------------------------------------------------------
    # Listener management
    # ------------------------------------------------------------------

    def add_listener(self, listener: EventListener) -> None:
        self._listeners.append(listener)

    def _emit(self, event: DeviceEvent) -> None:
        log.info("Event: %s", event)
        with self._lock:
            self._events.append(event)
        for listener in self._listeners:
            try:
                listener(event)
            except Exception:
                log.exception("Listener %r failed on %s", listener, event)

    @property
    def events(self) -> list[DeviceEvent]:
        """Return a snapshot of all events emitted so far."""
        with self._lock:
            return list(self._events)

    def events_of_type(self, *types: EventType) -> list[DeviceEvent]:
        """Return events matching any of the given types."""
        with self._lock:
            return [e for e in self._events if e.event_type in types]

    def clear_events(self) -> None:
        with self._lock:
            self._events.clear()

    # ------------------------------------------------------------------
    # BDF helper
    # ------------------------------------------------------------------

    @staticmethod
    def _bdf(dev: pb2.DeviceHealth) -> str:
        return dev.info.pci.bdf if dev.info.HasField("pci") else dev.info.uuid

    # ------------------------------------------------------------------
    # Response processing
    # ------------------------------------------------------------------

    def _process_response(
        self, response: pb2.DeviceHealthResponse, *, first: bool
    ) -> None:
        seen: set[str] = set()

        for dev in response.devices:
            bdf = self._bdf(dev)
            seen.add(bdf)

            if bdf not in self._known:
                etype = EventType.DEVICE_DISCOVERED if first else EventType.DEVICE_ADDED
                self._emit(
                    DeviceEvent(
                        event_type=etype,
                        bdf=bdf,
                        uuid=dev.info.uuid,
                        model=dev.info.model,
                    )
                )
                self._known[bdf] = dev

            self._check_health(dev, bdf)

        if not first:
            removed = set(self._known) - seen
            for bdf in removed:
                old = self._known.pop(bdf)
                for key in [k for k in self._unhealthy if k[0] == bdf]:
                    del self._unhealthy[key]
                self._emit(
                    DeviceEvent(
                        event_type=EventType.DEVICE_REMOVED,
                        bdf=bdf,
                        uuid=old.info.uuid,
                    )
                )
        else:
            self._known = {self._bdf(d): d for d in response.devices}

    def _check_health(self, dev: pb2.DeviceHealth, bdf: str) -> None:
        current_domains: set[str] = set()

        for hs in dev.health:
            current_domains.add(hs.name)
            key = (bdf, hs.name)
            sev = _PROTO_SEV_MAP.get(hs.severity, SeverityLevel.UNKNOWN)
            prev = self._unhealthy.get(key)

            if sev in _UNHEALTHY:
                if prev != sev:
                    self._unhealthy[key] = sev
                    event_type = _SEV_EVENT.get(sev, EventType.HEALTH_WARNING)

                    # Detect special event sub-types
                    specific_type = self._classify_event(
                        hs.name, hs.reason, hs.message, sev
                    )
                    self._emit(
                        DeviceEvent(
                            event_type=specific_type or event_type,
                            bdf=bdf,
                            uuid=dev.info.uuid,
                            domain=hs.name,
                            severity=sev,
                            reason=hs.reason,
                            message=hs.message,
                        )
                    )
            else:
                if key in self._unhealthy:
                    del self._unhealthy[key]
                    self._emit(
                        DeviceEvent(
                            event_type=EventType.HEALTH_RECOVERED,
                            bdf=bdf,
                            uuid=dev.info.uuid,
                            domain=hs.name,
                            severity=SeverityLevel.OK,
                        )
                    )

        stale = {
            k for k in self._unhealthy if k[0] == bdf and k[1] not in current_domains
        }
        for key in stale:
            del self._unhealthy[key]
            self._emit(
                DeviceEvent(
                    event_type=EventType.HEALTH_RECOVERED,
                    bdf=bdf,
                    uuid=dev.info.uuid,
                    domain=key[1],
                    severity=SeverityLevel.OK,
                )
            )

    @staticmethod
    def _classify_event(
        domain: str, reason: str, message: str, severity: SeverityLevel
    ) -> Optional[EventType]:
        """Map domain/reason/message to a specific EventType if applicable."""
        reason_lower = reason.lower()
        msg_lower = message.lower()

        # Pcode / device wedged
        if "pcode" in reason_lower or "pcode" in msg_lower:
            return EventType.PCODE_ERROR
        if "wedge" in reason_lower or "wedge" in msg_lower:
            return EventType.DEVICE_WEDGED

        # Survivability
        if "survivab" in reason_lower or "survivab" in msg_lower:
            return EventType.DEVICE_SURVIVABILITY

        # RAS
        if domain.startswith("ras") or "ras" in reason_lower:
            if "uncorrectable" in reason_lower or "uncorrectable" in msg_lower:
                return EventType.RAS_UNCORRECTABLE
            return EventType.RAS_CORRECTABLE

        # Throttle
        if "throttle" in reason_lower or "throttle" in msg_lower:
            if "thermal" in reason_lower or "thermal" in domain:
                return EventType.THERMAL_THROTTLE
            if "power" in reason_lower or "power" in domain:
                return EventType.POWER_THROTTLE
            return EventType.FREQUENCY_THROTTLE

        return None

    # ------------------------------------------------------------------
    # Background thread
    # ------------------------------------------------------------------

    def start(self, timeout: Optional[float] = None) -> None:
        """Start watching in a background thread."""
        if self._thread is not None and self._thread.is_alive():
            raise RuntimeError("Watcher already running")
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, args=(timeout,), daemon=True)
        self._thread.start()

    def stop(self, join_timeout: float = 5.0) -> None:
        """Signal the watcher to stop and wait for the thread to finish."""
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=join_timeout)

    def wait_for_event(
        self,
        event_type: EventType,
        *,
        bdf: Optional[str] = None,
        domain: Optional[str] = None,
        timeout: float = 60.0,
    ) -> Optional[DeviceEvent]:
        """Block until an event matching the criteria is emitted."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            with self._lock:
                for ev in reversed(self._events):
                    if ev.event_type != event_type:
                        continue
                    if bdf and ev.bdf != bdf:
                        continue
                    if domain and ev.domain != domain:
                        continue
                    return ev
            time.sleep(0.25)
        return None

    def _run(self, timeout: Optional[float]) -> None:
        first = True
        try:
            for response in self._client.watch_device_health(timeout=timeout):
                if self._stop.is_set():
                    break
                self._process_response(response, first=first)
                first = False
        except Exception:
            if not self._stop.is_set():
                log.exception("Health watcher stream error")
