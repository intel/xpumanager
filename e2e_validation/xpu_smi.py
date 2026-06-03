#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
xpu-smi CLI wrapper.

Provides a typed Python interface over the xpu-smi command-line tool,
used by validators to cross-check gRPC stream data against the CLI output.
"""

import json
import logging
import shlex
import shutil
import subprocess
from typing import Any, Optional

from .config import XpuSmiConfig

log = logging.getLogger(__name__)


class XpuSmiError(Exception):
    """Raised when xpu-smi returns a non-zero exit code."""

    def __init__(self, cmd: str, returncode: int, stderr: str):
        self.cmd = cmd
        self.returncode = returncode
        self.stderr = stderr
        super().__init__(f"xpu-smi failed (rc={returncode}): {stderr.strip()}")


class XpuSmiNotFound(XpuSmiError):
    """Raised when the xpu-smi binary cannot be located on $PATH."""

    def __init__(self, binary: str):
        self.binary = binary
        super().__init__(binary, 127, f"xpu-smi binary {binary!r} not found on $PATH")


class XpuSmi:
    """Typed wrapper around the xpu-smi CLI binary."""

    def __init__(self, config: Optional[XpuSmiConfig] = None) -> None:
        self._cfg = config or XpuSmiConfig()
        resolved = shutil.which(self._cfg.binary)
        if resolved is None:
            raise XpuSmiNotFound(self._cfg.binary)
        self._resolved_binary = resolved

    @classmethod
    def try_create(cls, config: Optional[XpuSmiConfig] = None) -> Optional["XpuSmi"]:
        """Return an XpuSmi instance, or ``None`` if the binary is unavailable.

        Quiet helper: the caller is responsible for logging the
        missing-binary condition (see ``run.py``), so this does not log
        to avoid duplicate warnings.
        """
        try:
            return cls(config)
        except XpuSmiNotFound:
            return None

    # ------------------------------------------------------------------
    # Low-level runner
    # ------------------------------------------------------------------

    def _run(self, args: list[str], *, json_output: bool = True) -> Any:
        """Execute xpu-smi with the given arguments and return parsed output."""
        cmd = [self._resolved_binary] + args
        if json_output and self._cfg.json_output and "-j" not in args:
            cmd.append("-j")

        log.debug("Running: %s", shlex.join(cmd))
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=self._cfg.timeout_s,
        )

        if result.returncode != 0:
            raise XpuSmiError(shlex.join(cmd), result.returncode, result.stderr)

        if json_output and self._cfg.json_output:
            try:
                return json.loads(result.stdout)
            except (json.JSONDecodeError, ValueError) as exc:
                raise XpuSmiError(
                    shlex.join(cmd),
                    result.returncode,
                    f"invalid JSON output: {exc}",
                ) from exc
        return result.stdout

    # ------------------------------------------------------------------
    # Discovery
    # ------------------------------------------------------------------

    def discovery(self) -> list[dict[str, Any]]:
        """List all discovered GPU devices."""
        return self._run(["discovery"])

    def discovery_device(self, device_id: int) -> dict[str, Any]:
        """Get detailed info for a single device."""
        return self._run(["discovery", "-d", str(device_id)])

    # ------------------------------------------------------------------
    # Health
    # ------------------------------------------------------------------

    def health(
        self, device_id: int, health_type: Optional[int] = None
    ) -> dict[str, Any]:
        """Query health status for a device."""
        args = ["health", "-d", str(device_id)]
        if health_type is not None:
            args.extend(["-c", str(health_type)])
        return self._run(args)

    def health_list(self, device_id: int) -> list[dict[str, Any]]:
        """List all health statuses for a device."""
        return self._run(["health", "-l", "-d", str(device_id)])

    # ------------------------------------------------------------------
    # Statistics & telemetry
    # ------------------------------------------------------------------

    def stats(self, device_id: int) -> dict[str, Any]:
        """Get runtime statistics for a device."""
        return self._run(["stats", "-d", str(device_id)])

    # ------------------------------------------------------------------
    # Firmware
    # ------------------------------------------------------------------

    def firmware_list(self, device_id: int) -> list[dict[str, Any]]:
        """List firmware versions for a device."""
        return self._run(["updatefw", "-d", str(device_id)])

    def firmware_update(
        self, device_id: int, fw_path: str, fw_type: str = "GFX"
    ) -> str:
        """Initiate firmware update on a device."""
        return self._run(
            ["updatefw", "-d", str(device_id), "-t", fw_type, "-f", fw_path],
            json_output=False,
        )

    # ------------------------------------------------------------------
    # Configuration
    # ------------------------------------------------------------------

    def set_frequency_range(
        self,
        device_id: int,
        tile_id: int = -1,
        min_freq: Optional[float] = None,
        max_freq: Optional[float] = None,
    ) -> str:
        """Set GPU frequency range in MHz."""
        args = ["config", "-d", str(device_id)]
        if tile_id >= 0:
            args.extend(["-t", str(tile_id)])
        if min_freq is not None and max_freq is not None:
            args.extend(["--frequencyrange", f"{min_freq},{max_freq}"])
        return self._run(args, json_output=False)

    def set_power_limit(self, device_id: int, power_w: float) -> str:
        """Set sustained power limit in watts."""
        return self._run(
            ["config", "-d", str(device_id), "--powerlimit", str(int(power_w))],
            json_output=False,
        )

    # ------------------------------------------------------------------
    # Reset
    # ------------------------------------------------------------------

    def reset_device(self, device_id: int, force: bool = False) -> str:
        """Perform a device reset."""
        args = ["reset", "-d", str(device_id)]
        if force:
            args.append("-f")
        return self._run(args, json_output=False)

    # ------------------------------------------------------------------
    # Policy
    # ------------------------------------------------------------------

    def set_policy(
        self,
        device_id: int,
        policy_type: int,
        condition: str,
        threshold: Optional[float] = None,
        action: Optional[str] = None,
    ) -> str:
        """Set a policy on a device."""
        args = [
            "policy",
            "-d",
            str(device_id),
            "--type",
            str(policy_type),
            "--condition",
            condition,
        ]
        if threshold is not None:
            args.extend(["--threshold", str(threshold)])
        if action is not None:
            args.extend(["--action", action])
        return self._run(args, json_output=False)

    def get_policies(self, device_id: int) -> Any:
        """List policies for a device."""
        return self._run(["policy", "-l", "-d", str(device_id)])

    # ------------------------------------------------------------------
    # Topology
    # ------------------------------------------------------------------

    def topology(self) -> Any:
        """Show GPU topology."""
        return self._run(["topology", "-m"])

    # ------------------------------------------------------------------
    # Diagnostics
    # ------------------------------------------------------------------

    def run_diag(self, device_id: int, level: int = 1) -> dict[str, Any]:
        """Run diagnostics on a device."""
        return self._run(["diag", "-d", str(device_id), "-l", str(level)])

    # ------------------------------------------------------------------
    # Groups
    # ------------------------------------------------------------------

    def create_group(self, name: str) -> Any:
        """Create a device group."""
        return self._run(["group", "-c", name], json_output=False)

    def add_to_group(self, group_id: int, device_id: int) -> Any:
        """Add a device to a group."""
        return self._run(
            ["group", "-a", "-g", str(group_id), "-d", str(device_id)],
            json_output=False,
        )

    def delete_group(self, group_id: int) -> Any:
        """Delete a device group."""
        return self._run(["group", "--delete", "-g", str(group_id)], json_output=False)

    def list_groups(self) -> Any:
        """List all device groups."""
        return self._run(["group", "-l"])
