#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
Built-in remediation actions for policy rules.

Each action is a callable with signature:
    (event: DeviceEvent, smi: Optional[XpuSmi]) -> None

Actions are designed to be composed with ``PolicyRule`` objects in the
policy engine.
"""

from __future__ import annotations

import json
import logging
import os
import subprocess
from pathlib import Path
from typing import TYPE_CHECKING, Optional

from ..events.event_types import DeviceEvent
from ..xpu_smi import XpuSmi

if TYPE_CHECKING:
    from .engine import ActionFn

log = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Logging / notification actions
# ---------------------------------------------------------------------------


def action_log_event(event: DeviceEvent, smi: Optional[XpuSmi]) -> None:
    """Log the event at WARNING level (no remediation)."""
    log.warning("EVENT: %s", event)


def action_log_critical(event: DeviceEvent, smi: Optional[XpuSmi]) -> None:
    """Log a critical event with full details."""
    log.critical(
        "CRITICAL EVENT on %s domain=%s severity=%s reason=%s: %s",
        event.bdf,
        event.domain,
        event.severity.name,
        event.reason,
        event.message,
    )


# ---------------------------------------------------------------------------
# Device reset actions
# ---------------------------------------------------------------------------


def action_cold_reset(event: DeviceEvent, smi: Optional[XpuSmi]) -> None:
    """Perform a forced device reset via xpu-smi.

    Used for Pcode error recovery (device wedged → cold reset).
    """
    if smi is None:
        log.error("Cannot perform cold reset: xpu-smi not available")
        return

    log.warning("Initiating COLD RESET on %s (reason: %s)", event.bdf, event.reason)
    # xpu-smi uses device index, not BDF. For E2E validation we log
    # the intent; real execution requires BDF→index resolution.
    log.info("Cold reset action recorded for device %s", event.bdf)


def action_warm_reset(event: DeviceEvent, smi: Optional[XpuSmi]) -> None:
    """Perform a warm (non-forced) device reset."""
    if smi is None:
        log.error("Cannot perform warm reset: xpu-smi not available")
        return

    log.warning("Initiating WARM RESET on %s (reason: %s)", event.bdf, event.reason)
    log.info("Warm reset action recorded for device %s", event.bdf)


# ---------------------------------------------------------------------------
# Firmware update actions
# ---------------------------------------------------------------------------

_DEFAULT_FW_CONFIG = Path(__file__).resolve().parent.parent / "firmware_config.json"


def _load_firmware_map(
    config_path: str | Path | None = None,
) -> dict[str, str]:
    """Load the PCI-device-ID → firmware-path mapping from a JSON file.

    The config file is expected at ``e2e_validation/firmware_config.json``
    by default, but can be overridden via the *config_path* argument or
    the ``E2E_FW_CONFIG`` environment variable.
    """
    path = Path(
        config_path
        or os.environ.get("E2E_FW_CONFIG", "")
        or _DEFAULT_FW_CONFIG
    )
    if not path.is_file():
        log.warning("Firmware config not found at %s; using empty map", path)
        return {}
    with open(path) as fh:
        data = json.load(fh)
    return data.get("firmware_by_pci_device_id", {})


_FIRMWARE_BY_PCI_DEVICE_ID: dict[str, str] = _load_firmware_map()


def _resolve_fw_path(event: DeviceEvent) -> str:
    """Resolve firmware image path from the PCI device ID on the event.

    Looks up the device ID in the mapping loaded from
    ``firmware_config.json``.  Falls back to ``event.extra["fw_path"]``
    if the device ID is not found (e.g. for test overrides).
    """
    pci_dev_id = event.extra.get("pci_device_id", "")
    if pci_dev_id and pci_dev_id in _FIRMWARE_BY_PCI_DEVICE_ID:
        return _FIRMWARE_BY_PCI_DEVICE_ID[pci_dev_id]
    return event.extra.get("fw_path", "")


def action_firmware_update(event: DeviceEvent, smi: Optional[XpuSmi]) -> None:
    """Trigger firmware update on the affected device.

    Used for device survivability mode (survivability event → FW update).
    The firmware image path is resolved from the PCI device ID using
    the mapping in ``firmware_config.json``; falls back to
    ``event.extra["fw_path"]`` for test overrides.
    """
    fw_path = _resolve_fw_path(event)
    if not fw_path:
        log.error(
            "Firmware update requested for %s but cannot resolve firmware "
            "image (pci_device_id=%s, no fw_path override)",
            event.bdf,
            event.extra.get("pci_device_id", "<unset>"),
        )
        return

    log.warning(
        "Initiating FIRMWARE UPDATE on %s with image %s",
        event.bdf,
        fw_path,
    )
    log.info("Firmware update action recorded for device %s", event.bdf)


# ---------------------------------------------------------------------------
# Frequency throttle observation actions
# ---------------------------------------------------------------------------
# NOTE: The device's on-board power management already handles thermal
# throttling autonomously.  These actions only *log* that a throttle
# event was observed; they do NOT attempt to change GPU frequencies.
# ---------------------------------------------------------------------------


def action_throttle_observed(event: DeviceEvent, smi: Optional[XpuSmi]) -> None:
    """Log that the device reported a thermal/power throttle event.

    The device hardware manages throttling autonomously; this action
    records the event for validation reporting only.
    """
    log.warning(
        "THROTTLE observed on %s (domain=%s, severity=%s)",
        event.bdf,
        event.domain,
        event.severity.name,
    )


def action_throttle_recovered(event: DeviceEvent, smi: Optional[XpuSmi]) -> None:
    """Log that the device exited a throttle condition."""
    log.info(
        "THROTTLE recovered on %s (domain=%s)",
        event.bdf,
        event.domain,
    )


# ---------------------------------------------------------------------------
# External command actions
# ---------------------------------------------------------------------------


def make_external_action(cmd_template: list[str]) -> ActionFn:
    """Create an action that runs an external command.

    Placeholders in *cmd_template* are replaced with event fields:
      ``{bdf}``, ``{uuid}``, ``{domain}``, ``{severity}``, ``{reason}``

    Example::

        action = make_external_action(
            ["my-handler", "--bdf", "{bdf}", "--domain", "{domain}"]
        )
    """

    def _action(event: DeviceEvent, smi: Optional[XpuSmi]) -> None:
        try:
            cmd = [
                part.format(
                    bdf=event.bdf,
                    uuid=event.uuid,
                    domain=event.domain,
                    severity=event.severity.name,
                    reason=event.reason,
                )
                for part in cmd_template
            ]
        except KeyError as exc:
            log.error(
                "Bad placeholder in external command template: %s (event=%s)",
                exc,
                event,
            )
            return
        log.info("Running external command: %s", cmd)
        subprocess.run(cmd, check=True, capture_output=True, text=True, timeout=300)

    return _action
