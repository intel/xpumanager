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
from ..xpu_smi import XpuSmi, XpuSmiError

if TYPE_CHECKING:
    from .engine import ActionFn

log = logging.getLogger(__name__)


class MissingInfrastructureError(RuntimeError):
    """Raised when an action cannot run because required infra is missing
    (e.g. xpu-smi binary not installed, firmware image not present on disk).

    The policy engine records this as a failed firing; validators must
    surface it as ERROR rather than silently passing.
    """


def _destructive_enabled() -> bool:
    """Whether to actually execute destructive ops (resets, FW flashes).

    Off by default — the validators verify the full call path
    (binary present, BDF resolves, FW image readable) but do *not*
    actually reset hardware or reflash firmware unless the operator
    opts in by setting ``E2E_DESTRUCTIVE=1``.
    """
    return os.environ.get("E2E_DESTRUCTIVE", "").lower() in ("1", "true", "yes")


def _require_smi(smi: Optional[XpuSmi], action: str, bdf: str) -> XpuSmi:
    if smi is None:
        raise MissingInfrastructureError(
            f"{action} requested for {bdf} but xpu-smi is not available"
        )
    return smi


def _bdf_to_device_index(smi: XpuSmi, bdf: str) -> int:
    """Resolve a PCI BDF (e.g. ``0000:03:00.0``) to the xpu-smi device index."""
    target = bdf.lower()
    try:
        devs = smi.discovery()
    except XpuSmiError as exc:
        raise MissingInfrastructureError(
            f"xpu-smi discovery failed while resolving {bdf}: {exc}"
        ) from exc
    # `xpu-smi discovery -j` may return a list, a {"device_list": [...]}
    # wrapper, or a single device dict.
    if isinstance(devs, dict):
        devs = devs.get("device_list") or devs.get("devices") or [devs]
    for d in devs:
        if not isinstance(d, dict):
            continue
        # xpu-smi field name is "pci_bdf_address"; tolerate "bdf" for older builds
        dev_bdf = str(d.get("pci_bdf_address", d.get("bdf", ""))).lower()
        if dev_bdf == target:
            return int(d["device_id"])
    raise MissingInfrastructureError(
        f"BDF {bdf} not present in xpu-smi discovery output"
    )


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


def action_cold_reset(event: DeviceEvent, smi: Optional[XpuSmi]) -> bool:
    """Perform a forced device reset via xpu-smi.

    Used for Pcode error recovery (device wedged → cold reset).
    By default this only *verifies* the call path (binary present,
    BDF resolves to a device index). Set ``E2E_DESTRUCTIVE=1`` to
    actually issue the reset.

    Returns ``True`` when the reset (or, by default, the verified call
    path) succeeds. Raises ``MissingInfrastructureError`` if xpu-smi is
    unavailable or the BDF cannot be resolved, and ``XpuSmiError`` if a
    destructive reset returns a non-zero exit code; both are recorded by
    the engine as a failed firing.
    """
    smi = _require_smi(smi, "cold reset", event.bdf)
    idx = _bdf_to_device_index(smi, event.bdf)
    if _destructive_enabled():
        log.warning(
            "Initiating COLD RESET on %s (idx=%d, reason: %s)",
            event.bdf, idx, event.reason,
        )
        smi.reset_device(idx, force=True)
    else:
        log.warning(
            "COLD RESET verified (not executed) on %s (idx=%d, reason: %s); "
            "set E2E_DESTRUCTIVE=1 to actually reset",
            event.bdf, idx, event.reason,
        )
    return True


def action_warm_reset(event: DeviceEvent, smi: Optional[XpuSmi]) -> bool:
    """Perform a warm (non-forced) device reset.

    Verifies xpu-smi reachability and BDF resolution. Set
    ``E2E_DESTRUCTIVE=1`` to actually issue the reset.

    Returns ``True`` on success; raises ``MissingInfrastructureError`` /
    ``XpuSmiError`` on failure (recorded as a failed firing).
    """
    smi = _require_smi(smi, "warm reset", event.bdf)
    idx = _bdf_to_device_index(smi, event.bdf)
    if _destructive_enabled():
        log.warning(
            "Initiating WARM RESET on %s (idx=%d, reason: %s)",
            event.bdf, idx, event.reason,
        )
        smi.reset_device(idx, force=False)
    else:
        log.warning(
            "WARM RESET verified (not executed) on %s (idx=%d, reason: %s)",
            event.bdf, idx, event.reason,
        )
    return True


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


def action_firmware_update(event: DeviceEvent, smi: Optional[XpuSmi]) -> bool:
    """Trigger firmware update on the affected device.

    Used for device survivability mode (survivability event → FW update).
    The firmware image path is resolved from the PCI device ID using
    the mapping in ``firmware_config.json``; falls back to
    ``event.extra["fw_path"]`` for test overrides.

    Returns ``True`` on success. Raises ``MissingInfrastructureError``
    if xpu-smi is not available, if no firmware image is mapped for the
    device, or if the resolved image file does not exist on disk, and
    ``XpuSmiError`` if a destructive flash returns a non-zero exit code.
    The engine records these as a failed firing so validators can
    surface ERROR rather than PASS.
    """
    smi = _require_smi(smi, "firmware update", event.bdf)

    fw_path = _resolve_fw_path(event)
    if not fw_path:
        raise MissingInfrastructureError(
            f"firmware update requested for {event.bdf} but no firmware image "
            f"is mapped (pci_device_id={event.extra.get('pci_device_id', '<unset>')!r}) "
            f"and no fw_path override was provided"
        )
    if not Path(fw_path).is_file():
        raise MissingInfrastructureError(
            f"firmware update requested for {event.bdf} but image not found on disk: {fw_path}"
        )

    idx = _bdf_to_device_index(smi, event.bdf)
    if _destructive_enabled():
        log.warning("Initiating FIRMWARE UPDATE on %s (idx=%d) with image %s",
                    event.bdf, idx, fw_path)
        smi.firmware_update(idx, fw_path)
    else:
        log.warning(
            "FIRMWARE UPDATE verified (not executed) on %s (idx=%d) with image %s",
            event.bdf, idx, fw_path,
        )
    return True


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
