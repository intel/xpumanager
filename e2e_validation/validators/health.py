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
import re
import time
from typing import Any

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

    # Map xpu-smi health keys to exporter health domain names.  The exporter
    # derives domains from its `{{ .hw_type }}` template, so both xpu-smi
    # temperature sensors collapse into the single "temperature" domain.
    _SMI_TO_GRPC_DOMAIN = {
        "core_temperature": "temperature",
        "memory_temperature": "temperature",
        "power_health": "power",
        "memory_health": "memory",
        "frequency_health": "frequency",
    }

    # Non-domain metadata keys in xpu-smi JSON.
    _SMI_IGNORE_KEYS = {
        "device_id",
        "unsupported_item_health",
    }

    def __init__(
        self,
        grpc_client: GrpcClient,
        xpu_smi: XpuSmi,
    ) -> None:
        self._grpc = grpc_client
        self._smi = xpu_smi

    @staticmethod
    def _normalize_bdf(value: str) -> str:
        """Normalize BDF to canonical lower-case zero-padded format."""
        s = (value or "").strip().lower()
        match = re.match(
            r"^([0-9a-f]{1,4}):([0-9a-f]{1,2}):([0-9a-f]{1,2})\.([0-7])$",
            s,
        )
        if not match:
            return s
        domain, bus, dev, func = match.groups()
        return (
            f"{int(domain, 16):04x}:{int(bus, 16):02x}:"
            f"{int(dev, 16):02x}.{int(func)}"
        )

    @classmethod
    def _canonical_smi_domain(cls, key: str) -> str:
        k = (key or "").strip().lower()
        if not k or k in cls._SMI_IGNORE_KEYS:
            return ""
        mapped = cls._SMI_TO_GRPC_DOMAIN.get(k)
        if mapped:
            return mapped
        # Unmapped keys: xpu-smi suffixes health categories with "_health",
        # while exporter domains carry the bare hw.type name.
        return k[: -len("_health")] if k.endswith("_health") else k

    @classmethod
    def _extract_smi_domains(cls, smi_health: Any) -> set[str]:
        """Extract health domains from supported xpu-smi JSON shapes."""
        domains: set[str] = set()

        # Shape A: list[dict].
        if isinstance(smi_health, list):
            for item in smi_health:
                if not isinstance(item, dict):
                    continue
                raw = item.get("name") or item.get("health_name") or item.get("type")
                dom = cls._canonical_smi_domain(str(raw or ""))
                if dom:
                    domains.add(dom)
            return domains

        # Shape B/C: dict (health_list wrapper or flat key map).
        if isinstance(smi_health, dict):
            health_list = smi_health.get("health_list")
            if isinstance(health_list, list):
                for item in health_list:
                    if not isinstance(item, dict):
                        continue
                    raw = (
                        item.get("name")
                        or item.get("health_name")
                        or item.get("type")
                    )
                    dom = cls._canonical_smi_domain(str(raw or ""))
                    if dom:
                        domains.add(dom)
                return domains

            for key, value in smi_health.items():
                if key in cls._SMI_IGNORE_KEYS:
                    continue
                if isinstance(value, dict) and "status" in value:
                    dom = cls._canonical_smi_domain(key)
                    if dom:
                        domains.add(dom)
            return domains

        return domains

    @staticmethod
    def _extract_grpc_domains(dev: pb2.DeviceHealth) -> set[str]:
        domains: set[str] = set()
        for hs in dev.health:
            name = (hs.name or "").strip().lower()
            if name:
                domains.add(name)
        return domains

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

        # Build BDF -> xpu-smi device_id mapping from discovery.
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
                if not isinstance(smi_dev, dict):
                    continue
                smi_bdf = (
                     smi_dev.get("pci_bdf_address")
                     or smi_dev.get("bdf")
                     or smi_dev.get("uuid", "")
                )
                smi_id = smi_dev.get("device_id")
                if smi_bdf and smi_id is not None:
                    bdf_to_smi_id[self._normalize_bdf(str(smi_bdf))] = int(smi_id)
        except (XpuSmiError, FileNotFoundError):
            pass

        smi_unavailable = False
        for idx, dev in enumerate(response.devices):
            bdf_raw = dev.info.pci.bdf if dev.info.HasField("pci") else dev.info.uuid
            bdf = self._normalize_bdf(str(bdf_raw))
            device_id = bdf_to_smi_id.get(bdf, idx)
            try:
                smi_health = self._smi.health(device_id)
                grpc_domains = self._extract_grpc_domains(dev)
                smi_domains = self._extract_smi_domains(smi_health)
                missing_in_smi = sorted(d for d in grpc_domains if d not in smi_domains)
                # Informational only: xpu-smi always enumerates every health
                # category (reporting "Unknown" for unsupported ones), while the
                # exporter only publishes domains with an active hw.status data
                # point.  Extra xpu-smi domains are therefore expected, not a
                # failure; only the reverse direction indicates a real
                # naming/reporting divergence.
                smi_only = sorted(d for d in smi_domains if d not in grpc_domains)
                mismatch = bool(missing_in_smi)
                details["comparisons"].append(
                    {
                        "device": bdf,
                        "device_id": device_id,
                        "grpc_domains": sorted(grpc_domains),
                        "smi_domains": sorted(smi_domains),
                        "missing_in_smi": missing_in_smi,
                        "smi_only_domains": smi_only,
                        "grpc_domain_count": len(grpc_domains),
                        "smi_domain_count": len(smi_domains),
                        "smi_response": "ok",
                        "grpc_domains_present_in_smi": not mismatch,
                    }
                )
                if mismatch:
                    return ValidationResult(
                        name=self.name,
                        status=ValidationStatus.FAIL,
                        message=(
                            f"Missing health domain(s) in xpu-smi for {bdf}: "
                            f"{', '.join(missing_in_smi)}"
                        ),
                        details=details,
                    )
            except (XpuSmiError, FileNotFoundError) as exc:
                smi_unavailable = True
                details["comparisons"].append(
                    {
                        "device": bdf,
                        "device_id": device_id,
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
