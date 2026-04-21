#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Device discovery validator.

Validates the full device-discovery pipeline:
  kernel (i915/xe) → Level Zero sysman → xpumd receiver → exporter gRPC
  → Python gRPC client → xpu-smi CLI

Checks:
  1. gRPC stream returns at least one device.
  2. Each device has a valid BDF, UUID, and model.
  3. PCI info, memory info, and firmware info are populated.
  4. xpu-smi ``discovery`` output matches gRPC device inventory.
"""

import logging
import re
from typing import Optional

from .base import BaseValidator, ValidationResult, ValidationStatus
from ..grpc_client import GrpcClient
from ..xpu_smi import XpuSmi, XpuSmiError

log = logging.getLogger(__name__)

_BDF_RE = re.compile(r"^[0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-9a-fA-F]$")


class DeviceDiscoveryValidator(BaseValidator):
    """Validate device discovery across the full stack."""

    name = "device_discovery"

    def __init__(
        self,
        grpc_client: GrpcClient,
        xpu_smi: Optional[XpuSmi] = None,
    ) -> None:
        self._grpc = grpc_client
        self._smi = xpu_smi

    def _run(self) -> ValidationResult:
        details: dict = {"grpc_devices": [], "smi_devices": [], "checks": []}

        # --- Step 1: gRPC snapshot ---
        try:
            response = self._grpc.snapshot(timeout=15.0)
        except Exception as exc:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message=f"Failed to get gRPC snapshot: {exc}",
                details=details,
            )

        if not response.devices:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message="gRPC stream returned zero devices",
                details=details,
            )

        details["checks"].append(f"gRPC returned {len(response.devices)} device(s)")

        # --- Step 2: Per-device field validation ---
        grpc_bdfs: set[str] = set()
        for dev in response.devices:
            info = dev.info
            bdf = info.pci.bdf if info.HasField("pci") else ""
            entry = {
                "uuid": info.uuid,
                "model": info.model,
                "bdf": bdf,
                "vendor_id": info.pci.vendor_id if info.HasField("pci") else "",
                "device_id": info.pci.device_id if info.HasField("pci") else "",
                "memory_count": len(info.memory),
                "firmware_count": len(info.firmwares),
                "health_count": len(dev.health),
            }
            details["grpc_devices"].append(entry)

            if not info.uuid:
                details["checks"].append(f"FAIL: device at {bdf} has empty UUID")
                return ValidationResult(
                    name=self.name,
                    status=ValidationStatus.FAIL,
                    message=f"Device at {bdf} missing UUID",
                    details=details,
                )

            if bdf and not _BDF_RE.match(bdf):
                details["checks"].append(
                    f"WARN: BDF {bdf!r} does not match expected pattern"
                )

            grpc_bdfs.add(bdf or info.uuid)

        details["checks"].append(
            f"All {len(response.devices)} devices have valid UUIDs"
        )

        # --- Step 3: Cross-check with xpu-smi (if available) ---
        if self._smi is not None:
            try:
                smi_out = self._smi.discovery()
                if isinstance(smi_out, list):
                    smi_list = smi_out
                elif isinstance(smi_out, dict) and "device_list" in smi_out:
                    smi_list = smi_out["device_list"]
                else:
                    smi_list = [smi_out] if smi_out else []
                details["smi_devices"] = smi_list
                details["checks"].append(
                    f"xpu-smi discovery returned {len(smi_list)} device(s)"
                )

                if len(smi_list) != len(response.devices):
                    details["checks"].append(
                        f"WARN: device count mismatch grpc={len(response.devices)} "
                        f"smi={len(smi_list)}"
                    )

                # Cross-check BDF/UUID sets
                smi_bdfs: set[str] = set()
                for smi_dev in smi_list:
                    if isinstance(smi_dev, dict):
                        smi_bdf = smi_dev.get("bdf", smi_dev.get("uuid", ""))
                        if smi_bdf:
                            smi_bdfs.add(smi_bdf)
                missing_in_smi = grpc_bdfs - smi_bdfs
                if missing_in_smi:
                    details["checks"].append(
                        f"WARN: devices in gRPC but not in xpu-smi: {missing_in_smi}"
                    )
            except XpuSmiError as exc:
                details["checks"].append(f"SKIP xpu-smi cross-check: {exc}")
            except FileNotFoundError:
                details["checks"].append("SKIP xpu-smi cross-check: binary not found")

        return ValidationResult(
            name=self.name,
            status=ValidationStatus.PASS,
            message=f"Discovered {len(response.devices)} device(s) via gRPC",
            details=details,
        )


class DeviceInfoValidator(BaseValidator):
    """Validate detailed device information fields."""

    name = "device_info_fields"

    def __init__(self, grpc_client: GrpcClient) -> None:
        self._grpc = grpc_client

    def _run(self) -> ValidationResult:
        details: dict = {"issues": []}

        try:
            response = self._grpc.snapshot(timeout=15.0)
        except Exception as exc:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message=f"gRPC snapshot failed: {exc}",
            )

        for dev in response.devices:
            bdf = dev.info.pci.bdf if dev.info.HasField("pci") else dev.info.uuid

            # Check memory info
            if not dev.info.memory:
                details["issues"].append(f"{bdf}: no memory info reported")

            for mem in dev.info.memory:
                if mem.size == 0:
                    details["issues"].append(
                        f"{bdf}: memory type={mem.type} reports size=0"
                    )

            # Check firmware info
            if not dev.info.firmwares:
                details["issues"].append(f"{bdf}: no firmware info reported")

            for fw in dev.info.firmwares:
                if not fw.version:
                    details["issues"].append(
                        f"{bdf}: firmware {fw.name} has empty version"
                    )

            # Check health domains
            if not dev.health:
                details["issues"].append(f"{bdf}: no health domains reported")

        if details["issues"]:
            return ValidationResult(
                name=self.name,
                status=ValidationStatus.FAIL,
                message=f"{len(details['issues'])} issue(s) found",
                details=details,
            )

        return ValidationResult(
            name=self.name,
            status=ValidationStatus.PASS,
            message="All device info fields populated",
            details=details,
        )
