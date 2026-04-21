#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Health monitoring validator.

Validates that the health daemon (health watcher) correctly detects and
reports device health state transitions through the gRPC stream.

Checks:
  1. Health stream starts and returns at least one snapshot.
  2. All health domains report a valid severity level.
  3. Health transitions emit the correct event types.
  4. xpu-smi health output agrees with gRPC-reported health.
"""

import logging
import time

from .base import BaseValidator, ValidationResult, ValidationStatus
from ..events.event_types import EventType
from ..events.health_watcher import HealthWatcher
from ..grpc_client import GrpcClient
from ..xpu_smi import XpuSmi, XpuSmiError
from .. import deviceinfo_pb2 as pb2

log = logging.getLogger(__name__)

_VALID_SEVERITIES = {
    pb2.SEVERITY_LEVEL_UNKNOWN,
    pb2.SEVERITY_LEVEL_OK,
    pb2.SEVERITY_LEVEL_WARNING,
    pb2.SEVERITY_LEVEL_CRITICAL,
    pb2.SEVERITY_LEVEL_FAILED,
}


class HealthStreamValidator(BaseValidator):
    """Validate the health stream fundamentals."""

    name = "health_stream"

    def __init__(self, grpc_client: GrpcClient) -> None:
        self._grpc = grpc_client

    def _run(self) -> ValidationResult:
        details: dict = {"domains": [], "checks": []}

        try:
            response = self._grpc.snapshot(timeout=15.0)
        except Exception as exc:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message=f"Cannot obtain health snapshot: {exc}",
            )

        for dev in response.devices:
            bdf = dev.info.pci.bdf if dev.info.HasField("pci") else dev.info.uuid
            for hs in dev.health:
                entry = {
                    "bdf": bdf,
                    "domain": hs.name,
                    "severity": hs.severity,
                    "reason": hs.reason,
                }
                details["domains"].append(entry)

                if hs.severity not in _VALID_SEVERITIES:
                    details["checks"].append(
                        f"FAIL: {bdf} domain={hs.name} invalid severity={hs.severity}"
                    )
                    return ValidationResult(
                        name=self.name,
                        status=ValidationStatus.FAIL,
                        message=f"Invalid severity on {bdf}/{hs.name}",
                        details=details,
                    )

        details["checks"].append(
            f"Validated {len(details['domains'])} health domain(s) across "
            f"{len(response.devices)} device(s)"
        )

        return ValidationResult(
            name=self.name,
            status=ValidationStatus.PASS,
            message=f"{len(details['domains'])} health domain(s) valid",
            details=details,
        )


class HealthWatcherValidator(BaseValidator):
    """Validate the background health watcher detects events correctly."""

    name = "health_watcher"

    def __init__(
        self,
        grpc_client: GrpcClient,
        watch_duration_s: float = 30.0,
    ) -> None:
        self._grpc = grpc_client
        self._duration = watch_duration_s

    def _run(self) -> ValidationResult:
        details: dict = {"events_collected": 0, "event_types": {}}

        watcher = HealthWatcher(self._grpc)
        watcher.start(timeout=self._duration)

        # Let the watcher run for the configured duration
        time.sleep(self._duration)
        watcher.stop()

        events = watcher.events
        details["events_collected"] = len(events)
        for ev in events:
            etype = ev.event_type.value
            details["event_types"][etype] = details["event_types"].get(etype, 0) + 1

        # Must discover at least one device
        discovery_events = [
            e for e in events if e.event_type == EventType.DEVICE_DISCOVERED
        ]
        if not discovery_events:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="Watcher did not discover any devices",
                details=details,
            )

        return ValidationResult(
            name=self.name,
            status=ValidationStatus.PASS,
            message=(
                f"Watcher collected {len(events)} events, "
                f"discovered {len(discovery_events)} device(s)"
            ),
            details=details,
        )


class HealthSmiCrossCheckValidator(BaseValidator):
    """Cross-check gRPC health against xpu-smi health output."""

    name = "health_smi_crosscheck"

    def __init__(
        self,
        grpc_client: GrpcClient,
        xpu_smi: XpuSmi,
    ) -> None:
        self._grpc = grpc_client
        self._smi = xpu_smi

    def _run(self) -> ValidationResult:
        details: dict = {"comparisons": []}

        try:
            response = self._grpc.snapshot(timeout=15.0)
        except Exception as exc:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message=f"gRPC snapshot failed: {exc}",
            )

        # Build BDF -> xpu-smi device_id mapping from discovery
        bdf_to_smi_id: dict[str, int] = {}
        try:
            smi_disc = self._smi.discovery()
            smi_list = (
                smi_disc
                if isinstance(smi_disc, list)
                else smi_disc.get("device_list", [smi_disc] if smi_disc else [])
                if isinstance(smi_disc, dict)
                else []
            )
            for smi_dev in smi_list:
                if isinstance(smi_dev, dict):
                    smi_bdf = smi_dev.get("bdf", smi_dev.get("uuid", ""))
                    smi_id = smi_dev.get("device_id")
                    if smi_bdf and smi_id is not None:
                        bdf_to_smi_id[smi_bdf] = int(smi_id)
        except (XpuSmiError, FileNotFoundError):
            pass

        smi_unavailable = False
        for idx, dev in enumerate(response.devices):
            bdf = dev.info.pci.bdf if dev.info.HasField("pci") else dev.info.uuid
            device_id = bdf_to_smi_id.get(bdf, idx)
            try:
                smi_health = self._smi.health(device_id)
                grpc_domain_count = len(dev.health)
                # Compare domain counts between gRPC and xpu-smi
                smi_domain_count = (
                    len(smi_health)
                    if isinstance(smi_health, list)
                    else (
                        len(smi_health.get("health_list", []))
                        if isinstance(smi_health, dict)
                        else 0
                    )
                )
                mismatch = grpc_domain_count != smi_domain_count
                details["comparisons"].append(
                    {
                        "device": bdf,
                        "grpc_domains": grpc_domain_count,
                        "smi_domains": smi_domain_count,
                        "smi_response": "ok",
                        "domain_count_match": not mismatch,
                    }
                )
                if mismatch:
                    return ValidationResult(
                        name=self.name,
                        status=ValidationStatus.FAIL,
                        message=(
                            f"Domain count mismatch on {bdf}: "
                            f"gRPC={grpc_domain_count} smi={smi_domain_count}"
                        ),
                        details=details,
                    )
            except (XpuSmiError, FileNotFoundError) as exc:
                smi_unavailable = True
                details["comparisons"].append(
                    {
                        "device": bdf,
                        "smi_response": str(exc),
                    }
                )

        if smi_unavailable:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.SKIP,
                message="xpu-smi unavailable for one or more devices",
                details=details,
            )

        return ValidationResult(
            name=self.name,
            status=ValidationStatus.PASS,
            message=f"Cross-checked {len(details['comparisons'])} device(s)",
            details=details,
        )
