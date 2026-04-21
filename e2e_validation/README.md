# E2E Validation Framework for XPU Manager

End-to-end validation tool that tests the complete stack:

```
kernel (i915/xe) → Level Zero sysman → xpumd → local gRPC API → Python script → xpu-smi
```

## Architecture

```
e2e_validation/
├── config.py                 # Central configuration
├── grpc_client.py            # gRPC client wrapper for xpuinfo socket
├── xpu_smi.py                # xpu-smi CLI wrapper
├── deviceinfo_pb2.py         # Generated protobuf stubs
├── deviceinfo_pb2_grpc.py    # Generated gRPC stubs
├── events/
│   ├── event_types.py        # Event & severity definitions
│   └── health_watcher.py     # Background health stream watcher
├── policies/
│   ├── engine.py             # Policy engine (event → action mapping)
│   ├── actions.py            # Built-in remediation actions
│   └── presets.py            # Pre-built rule sets for common use cases
├── validators/
│   ├── base.py               # Base validator & result types
│   ├── discovery.py          # Device discovery validation
│   ├── health.py             # Health monitoring validation
│   └── use_cases.py          # Use-case-specific validators
├── tests/
│   ├── test_policy_engine.py # Policy engine unit tests
│   ├── test_health_watcher.py# Health watcher unit tests
│   ├── test_use_cases.py     # Use-case validator tests
│   └── test_presets.py       # Preset rule set tests
├── run.py                    # Main entry point & CLI
└── requirements.txt          # Python dependencies
```

## Requirements

- Python 3.10+
- `grpcio >= 1.78.0`
- `protobuf >= 6.31.1`
- `pytest >= 7.0.0` (for running tests)

## Installation

```bash
cd <repo-root>
pip install -r e2e_validation/requirements.txt
```

## Usage

### Offline Validation (no live services required)

Runs all use-case validators with synthetic events to verify policy
wiring and logic:

```bash
# Run all offline validators
python -m e2e_validation

# Run a specific validator
python -m e2e_validation --only pcode_error_recovery

# Verbose output
python -m e2e_validation -v
```

### Live Validation (requires running xpumd + xpu-smi)

Connects to the xpuinfo exporter gRPC socket and cross-checks against
xpu-smi:

```bash
# Default socket location
python -m e2e_validation --live

# Custom socket
python -m e2e_validation --live --sock-dir /run/xpumd --sock-name xpuinfo.sock
```

### Run Unit Tests

```bash
cd <repo-root>/e2e_validation
pytest -v
```

## Validated Use Cases

### 1. Pcode Error Recovery (`pcode_error_recovery`)
- **Flow:** Pcode error → device wedged → cold reset → recovery
- **Validates:** Policy engine detects PCODE_ERROR / DEVICE_WEDGED events and triggers cold reset action with cooldown/retry limits

### 2. Device Survivability (`device_survivability`)
- **Flow:** Survivability event → firmware update
- **Validates:** Survivability mode detection triggers FW update action (limited to 1 attempt)

### 3. RAS Events (`ras_events`)
- **Flow:** Correctable → log; Uncorrectable WARNING → warm reset; Uncorrectable CRITICAL → cold reset
- **Validates:** Graduated response to RAS error categories

### 4. Group Policy (`group_policy`)
- **Flow:** Health event on any device in group → applies same policy to all
- **Validates:** Policy rules fire for all devices in the group

### 5. Thermal Throttle Recovery (`thermal_throttle_recovery`)
- **Flow:** Thermal warning → frequency throttle → recovery → restore defaults
- **Validates:** Automatic frequency management based on thermal events

### 6. Severity Escalation (`severity_escalation`)
- **Flow:** WARNING → CRITICAL (re-fires policy at each level)
- **Validates:** Severity changes trigger new policy evaluations

### 7. Device Lifecycle (`device_lifecycle`)
- **Flow:** Hot-plug (device added) → log; Hot-unplug (device removed) → alert
- **Validates:** Device appearance/disappearance detection

### 8. Policy Cooldown (`policy_cooldown`)
- **Validates:** Duplicate events within cooldown period are suppressed

### 9. Max Fires Limit (`policy_max_fires`)
- **Validates:** Policy rules respect maximum firing count limits

## Policy Engine

The policy engine maps events to actions using configurable rules:

```python
from e2e_validation.policies.engine import PolicyEngine, PolicyRule
from e2e_validation.policies.actions import action_cold_reset
from e2e_validation.events.event_types import EventType, SeverityLevel

engine = PolicyEngine()
engine.add_rule(PolicyRule(
    name="my_custom_rule",
    event_types={EventType.DEVICE_WEDGED},
    min_severity=SeverityLevel.CRITICAL,
    action=action_cold_reset,
    cooldown_s=30.0,
    max_fires=3,
))
```

### Custom Actions

Actions are simple callables:

```python
def my_action(event, smi):
    print(f"Device {event.bdf} domain={event.domain} severity={event.severity.name}")
    # Call xpu-smi, external binary, send alert, etc.
```

### External Command Actions

```python
from e2e_validation.policies.actions import make_external_action

action = make_external_action(
    ["my-handler", "--bdf", "{bdf}", "--domain", "{domain}", "--severity", "{severity}"]
)
```

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `XDG_RUNTIME_DIR` | `/run/user/0` | Default socket directory |
| `XPU_SMI_BIN` | `xpu-smi` | Path to xpu-smi binary |
| `E2E_LOG_DIR` | `/tmp/xpum_e2e_logs` | Log output directory |
| `E2E_VERBOSE` | `false` | Enable verbose logging |
| `E2E_SOCK_DIR` | `$XDG_RUNTIME_DIR` | Override socket directory |
| `E2E_SOCK_NAME` | `intelxpuinfo.sock` | Override socket filename |
