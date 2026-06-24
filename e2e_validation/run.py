#
# Copyright (C) 2026 Intel Corporation
#
# SPDX-License-Identifier: MIT

"""
E2E Validation runner — main entry point.

Runs all validators (or a selected subset) and prints a summary report.
Can operate in two modes:

  **Offline mode** (default): Runs use-case validators with synthetic
  events to verify policy wiring and logic.  No live gRPC connection
  or xpu-smi binary required.

  **Live mode** (``--live``): Runs *only* live validators.  Connects to
  the xpuinfo exporter socket and validates device discovery, health
  streaming, and cross-checks against xpu-smi, then waits for *real*
  KMD-sourced events on the stream (Pcode/RAS/survivability/thermal/
  health).  Event-driven validators FAIL if no real event arrives within
  ``--live-timeout`` seconds.  No synthetic events are produced in this
  mode.

Usage::

    # Offline validation (synthetic events)
    python -m e2e_validation.run

    # Live validation (requires running xpumd + xpu-smi)
    python -m e2e_validation.run --live

    # Run a specific validator
    python -m e2e_validation.run --only pcode_error_recovery

    # Run with custom socket
    python -m e2e_validation.run --live --sock-dir /run/xpumd
"""

import argparse
import logging
import sys
from typing import Optional

from .config import ValidationConfig
from .validators.base import ValidationResult, ValidationStatus
from .xpu_smi import XpuSmi

log = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Validator registry
# ---------------------------------------------------------------------------


def _get_offline_validators(smi: Optional[XpuSmi] = None):
    """Return validators that work without a live gRPC connection.

    *smi* is optional and is propagated to validators that exercise
    remediation actions (cold-reset, firmware update, etc.).  If it is
    ``None`` those validators will report ERROR rather than PASS.
    """
    from .validators.use_cases import (
        PcodeErrorRecoveryValidator,
        DeviceSurvivabilityValidator,
        RASEventValidator,
        GroupPolicyValidator,
        ThermalThrottleValidator,
        SeverityEscalationValidator,
        DeviceLifecycleValidator,
        PolicyCooldownValidator,
        MaxFiresValidator,
    )

    return [
        PcodeErrorRecoveryValidator(xpu_smi=smi),
        DeviceSurvivabilityValidator(xpu_smi=smi),
        RASEventValidator(xpu_smi=smi),
        GroupPolicyValidator(xpu_smi=smi),
        ThermalThrottleValidator(xpu_smi=smi),
        SeverityEscalationValidator(xpu_smi=smi),
        DeviceLifecycleValidator(xpu_smi=smi),
        PolicyCooldownValidator(),
        MaxFiresValidator(),
    ]


def _get_live_validators(config: ValidationConfig, smi: XpuSmi):
    """Return validators that require a live gRPC connection.

    These consume **only real events/data** from the live stream — they
    never synthesize events.  They fall into two groups:

      * *Snapshot* validators (discovery, device-info, health-stream,
        smi cross-check) inspect the current device inventory/health,
        which the daemon always reports.
      * *Event-driven* validators (:mod:`validators.live_events`) wait up
        to ``live_event_timeout_s`` for a real KMD-sourced event and FAIL
        if none arrives.  They share a single background stream session
        so the run is bounded by one timeout window, not one per check.

    Returns ``(validators, client, session)`` where *session* is the
    shared :class:`LiveStreamSession` (or ``None``) and must be stopped
    by the caller.
    """
    from .grpc_client import GrpcClient
    from .validators.discovery import DeviceDiscoveryValidator, DeviceInfoValidator
    from .validators.health import (
        HealthStreamValidator,
        HealthWatcherValidator,
        HealthSmiCrossCheckValidator,
    )
    from .validators.live_events import (
        LiveStreamSession,
        LiveHealthEventValidator,
        LivePcodeErrorValidator,
        LiveRASValidator,
        LiveSurvivabilityValidator,
        LiveThermalThrottleValidator,
    )

    client = GrpcClient(config.socket)
    client.connect()

    # If anything below raises (session/validator construction), close the
    # channel we just opened so the connected client is not leaked.
    try:
        session = LiveStreamSession(client, config.socket.live_event_timeout_s)

        validators = [
            # Snapshot validators — inspect real inventory/health, no synthesis.
            DeviceDiscoveryValidator(client, smi),
            DeviceInfoValidator(client),
            HealthStreamValidator(client),
            HealthWatcherValidator(client),
            HealthSmiCrossCheckValidator(client, smi),
            # Event-driven validators — wait for real KMD events, FAIL on timeout.
            LiveHealthEventValidator(session),
            LivePcodeErrorValidator(session),
            LiveRASValidator(session),
            LiveSurvivabilityValidator(session),
            LiveThermalThrottleValidator(session),
        ]
    except Exception:
        client.close()
        raise
    return validators, client, session


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------


def _print_report(results: list[ValidationResult]) -> int:
    """Print a summary report and return the exit code."""
    print("\n" + "=" * 72)
    print("  E2E VALIDATION REPORT")
    print("=" * 72)

    passed = failed = skipped = errors = 0
    for r in results:
        status_icon = {
            ValidationStatus.PASS: "✓",
            ValidationStatus.FAIL: "✗",
            ValidationStatus.SKIP: "–",
            ValidationStatus.ERROR: "!",
        }[r.status]

        print(f"  {status_icon} {r.name:<35} {r.status.value:<6} {r.message}")

        if r.status == ValidationStatus.PASS:
            passed += 1
        elif r.status == ValidationStatus.FAIL:
            failed += 1
        elif r.status == ValidationStatus.SKIP:
            skipped += 1
        else:
            errors += 1

    total = len(results)
    print("-" * 72)
    print(
        f"  Total: {total}  |  Passed: {passed}  |  Failed: {failed}  "
        f"|  Skipped: {skipped}  |  Errors: {errors}"
    )
    print("=" * 72 + "\n")

    return 0 if (failed + errors) == 0 else 1


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def _parse_args(argv: Optional[list[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="E2E validation for the XPU Manager stack",
    )
    parser.add_argument(
        "--live",
        action="store_true",
        help=(
            "Run live validators only (requires running xpumd + xpu-smi). "
            "Live validators consume real events from the gRPC stream and "
            "never synthesize events. Offline/synthetic validators are NOT "
            "run in this mode."
        ),
    )
    parser.add_argument(
        "--live-timeout",
        type=float,
        metavar="SECONDS",
        default=None,
        help=(
            "How long event-driven live validators wait for a real event "
            "before reporting FAIL (default: 300s). Only meaningful with "
            "--live."
        ),
    )
    parser.add_argument(
        "--only",
        nargs="*",
        metavar="NAME",
        help=(
            "Run only validators whose names contain these substrings "
            "(per-test-case selection). Works in both offline and --live "
            "modes, e.g. '--live --only live_pcode_error'."
        ),
    )
    parser.add_argument(
        "--sock-dir",
        help="Directory containing the gRPC Unix socket",
    )
    parser.add_argument(
        "--sock-name",
        help="Socket file name",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Enable verbose logging",
    )
    return parser.parse_args(argv)


def main(argv: Optional[list[str]] = None) -> int:
    args = _parse_args(argv)

    level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(
        format="%(asctime)s %(levelname)-8s %(name)s: %(message)s",
        level=level,
    )

    config = ValidationConfig.from_env()
    if args.sock_dir:
        config.socket.sock_dir = args.sock_dir
    if args.sock_name:
        config.socket.sock_name = args.sock_name
    if args.live_timeout is not None:
        config.socket.live_event_timeout_s = args.live_timeout

    # Try to locate xpu-smi for both offline (action targets) and live mode.
    smi = XpuSmi.try_create(config.xpu_smi)
    if smi is None:
        log.warning(
            "xpu-smi binary not found on $PATH; validators that require it "
            "will report ERROR"
        )

    client = None
    session = None
    if args.live:
        # Live mode is exclusive: run ONLY live validators, which consume
        # real events from the stream. Offline/synthetic validators are not
        # run here — they belong to the default (offline) mode.
        if smi is None:
            log.error(
                "--live requires xpu-smi but it was not found on $PATH; aborting"
            )
            return 2
        try:
            validators, client, session = _get_live_validators(config, smi)
        except Exception as exc:
            log.error("Cannot start live validators: %s", exc)
            return 2
    else:
        # Offline mode: synthetic-event validators that exercise policy
        # wiring without a live connection.
        validators = _get_offline_validators(smi=smi)

    # Filter by --only (per-test-case selection, both modes)
    if args.only:
        validators = [v for v in validators if any(sub in v.name for sub in args.only)]
        if not validators:
            log.error("No validators match --only %s", args.only)
            if session is not None:
                session.stop()
            if client is not None:
                client.close()
            return 1

    # Run
    results: list[ValidationResult] = []
    try:
        if session is not None:
            # Live mode: run the snapshot validators first (some, e.g.
            # health_watcher, block for ~30s), then arm the shared event
            # stream immediately *before* the event-driven validators so
            # they get the full --live-timeout window rather than having it
            # eaten by the snapshot checks.
            from .validators.live_events import EventDrivenValidator

            event_validators = [
                v for v in validators if isinstance(v, EventDrivenValidator)
            ]
            snapshot_validators = [
                v for v in validators if not isinstance(v, EventDrivenValidator)
            ]
            for v in snapshot_validators:
                results.append(v.run())
            if event_validators:
                session.start()
                for v in event_validators:
                    results.append(v.run())
        else:
            for v in validators:
                results.append(v.run())
    finally:
        # Cleanup
        if session is not None:
            session.stop()
        if client is not None:
            client.close()

    return _print_report(results)


if __name__ == "__main__":
    sys.exit(main())
