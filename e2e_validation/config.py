#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
E2E Validation Configuration.

Central configuration for socket paths, timeouts, thresholds, xpu-smi
binary location, and policy defaults used across all validators.
"""

import os
from dataclasses import dataclass, field


@dataclass
class SocketConfig:
    """gRPC Unix socket connection settings."""

    sock_dir: str = field(
        default_factory=lambda: os.environ.get("XDG_RUNTIME_DIR", "/run/user/0")
    )
    sock_name: str = "intelxpuinfo.sock"
    connect_timeout_s: float = 10.0
    stream_timeout_s: float = 300.0

    @property
    def socket_path(self) -> str:
        return os.path.join(self.sock_dir, self.sock_name)

    @property
    def target(self) -> str:
        return f"unix://{self.socket_path}"


@dataclass
class XpuSmiConfig:
    """xpu-smi CLI binary settings."""

    binary: str = field(
        default_factory=lambda: os.environ.get("XPU_SMI_BIN", "xpu-smi")
    )
    timeout_s: float = 30.0
    json_output: bool = True


@dataclass
class HealthThresholds:
    """Default health thresholds for validation."""

    core_thermal_warning_c: float = 85.0
    core_thermal_critical_c: float = 100.0
    memory_thermal_warning_c: float = 90.0
    memory_thermal_critical_c: float = 105.0
    power_warning_w: float = 300.0
    power_critical_w: float = 350.0


@dataclass
class PolicyDefaults:
    """Default settings for policy-based validation."""

    poll_interval_s: float = 2.0
    max_wait_s: float = 120.0
    recovery_cooldown_s: float = 10.0
    max_retries: int = 3


@dataclass
class ValidationConfig:
    """Top-level configuration aggregating all sub-configs."""

    socket: SocketConfig = field(default_factory=SocketConfig)
    xpu_smi: XpuSmiConfig = field(default_factory=XpuSmiConfig)
    thresholds: HealthThresholds = field(default_factory=HealthThresholds)
    policy: PolicyDefaults = field(default_factory=PolicyDefaults)
    log_dir: str = field(
        default_factory=lambda: os.environ.get("E2E_LOG_DIR", "/tmp/xpum_e2e_logs")
    )
    verbose: bool = False

    @classmethod
    def from_env(cls) -> "ValidationConfig":
        """Build configuration from environment variables."""
        cfg = cls()
        if os.environ.get("E2E_VERBOSE", "").lower() in ("1", "true", "yes"):
            cfg.verbose = True
        sock_dir = os.environ.get("E2E_SOCK_DIR")
        if sock_dir:
            cfg.socket.sock_dir = sock_dir
        sock_name = os.environ.get("E2E_SOCK_NAME")
        if sock_name:
            cfg.socket.sock_name = sock_name
        return cfg
