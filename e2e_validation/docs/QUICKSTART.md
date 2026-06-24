# E2E Validation — Quick Start (every scenario)

How to run the `e2e_validation` framework after the live-events change.
For the design rationale and fault-injection options, see
[live-events-design.md](live-events-design.md).

> **Key concept (new):** `--live` and offline are now **mutually exclusive**.
> - **Offline** (default, no `--live`): runs the 9 synthetic/policy validators,
>   no daemon needed. Tests policy-engine logic with fabricated events.
> - **`--live`**: runs **only** the live validators, which consume **real**
>   events from the gRPC stream and never synthesize. Needs a running `xpumd`
>   + `xpu-smi`.
> - **"Both"** = run the two commands back to back (there is no single combined
>   mode by design).

> **Placeholders used below.** Substitute for your environment:
> | Placeholder | Meaning | Example (the `gta` test box) |
> |-------------|---------|------------------------------|
> | `<repo-root>` | repo checkout dir | `/home/gta/xpumd/libraries.compute.xpu-manager.xpum` |
> | `<venv-python>` | python inside your venv | `/home/gta/xpumd/myvenv/bin/python` |
> | `<xpu-smi-or-xpumcli>` | the SMI CLI on the box | `xpumcli` (this box has no `xpu-smi`) |

---

## 0. One-time setup

```bash
cd <repo-root>
python -m venv .venv && source .venv/bin/activate   # venv with grpcio/protobuf
pip install -r e2e_validation/requirements.txt       # first time only
# (optional) git checkout <branch-or-tag>
```

---

## 1. Unit tests (no hardware, no daemon)

```bash
cd <repo-root>
python -m pytest e2e_validation/tests/ -q
```
Expect: all tests pass.

---

## 2. Offline validation (synthetic events, no daemon)

Validates policy-engine wiring. No socket required.

```bash
# All 9 offline validators
python -m e2e_validation

# Verbose
python -m e2e_validation -v

# A single offline case
python -m e2e_validation --only group_policy
```

Runs: `pcode_error_recovery, device_survivability, ras_events, group_policy,
thermal_throttle_recovery, severity_escalation, device_lifecycle,
policy_cooldown, policy_max_fires`.

> ⚠️ **xpu-smi gotcha (offline):** the action validators call `xpu-smi
> discovery`. On a box with a real `/usr/bin/xpu-smi` but no working GPU, that
> call can hang. Point it at an available CLI (or a non-existent path to make
> those validators ERROR instead of hang):
> ```bash
> XPU_SMI_BIN=<xpu-smi-or-xpumcli> python -m e2e_validation
> ```

---

## 3. Live validation (real events, needs daemon)

### 3a. Start the daemon (terminal 1, leave running)
```bash
cd <repo-root>/xpumd/dist
sudo ./xpumd --config ../config-example.yaml
```
Wait for: `gRPC server listening ... endpoint: /run/xpumd/intelxpuinfo.sock`.

### 3b. Run live validators (terminal 2)
The socket is `root:root` (0660), so the client runs as **root**, using the
venv python **by full path** (under `sudo`, a venv must be selected by the
binary you launch — `source activate` only edits `$PATH`, which `sudo` resets),
with `XPU_SMI_BIN` set if the box ships `xpumcli` rather than `xpu-smi`:

```bash
cd <repo-root>
sudo env XPU_SMI_BIN=<xpu-smi-or-xpumcli> <venv-python> \
    -m e2e_validation --live \
    --sock-dir /run/xpumd --sock-name intelxpuinfo.sock
```

Runs 10 live validators:
- **Snapshot** (pass on real inventory/health, instant): `device_discovery`,
  `device_info_fields`, `health_stream`, `health_watcher`,
  `health_smi_crosscheck`.
- **Event-driven** (wait up to 5 min for a *real* event, else FAIL):
  `live_health_event`, `live_pcode_error`, `live_ras_events`,
  `live_survivability`, `live_thermal_throttle`.

Expect: `Connected to unix:///run/xpumd/intelxpuinfo.sock`, snapshot validators
PASS, and — on healthy hardware with no fault induced — the event-driven ones
**FAIL on timeout** (that is correct behaviour; they need a real fault).

> **Run as your user instead of root** (optional): after starting the daemon,
> `sudo chown root:render /run/xpumd/intelxpuinfo.sock` (if you're in `render`),
> then run without `sudo` using a normal activated venv. Redo the chown after
> each daemon restart.

---

## 4. A single live test case (`--only`)

```bash
# Just the Pcode event validator
sudo env XPU_SMI_BIN=<xpu-smi-or-xpumcli> <venv-python> \
    -m e2e_validation --live --only live_pcode_error \
    --sock-dir /run/xpumd --sock-name intelxpuinfo.sock

# Just the always-on snapshot checks (fast, good smoke test)
sudo env XPU_SMI_BIN=<xpu-smi-or-xpumcli> <venv-python> \
    -m e2e_validation --live --only device_discovery health_stream \
    --sock-dir /run/xpumd --sock-name intelxpuinfo.sock
```
`--only` matches by substring and works in both modes.

---

## 5. Adjust the event wait timeout

Default is 300 s (5 min). Shorten it when you're driving a fault yourself, or
to fail fast:

```bash
# Flag form
sudo env XPU_SMI_BIN=<xpu-smi-or-xpumcli> <venv-python> \
    -m e2e_validation --live --live-timeout 60 \
    --sock-dir /run/xpumd --sock-name intelxpuinfo.sock

# Env form (equivalent)
sudo env XPU_SMI_BIN=<xpu-smi-or-xpumcli> E2E_LIVE_EVENT_TIMEOUT=60 \
    <venv-python> -m e2e_validation --live \
    --sock-dir /run/xpumd --sock-name intelxpuinfo.sock
```

---

## 6. Drive a real fault during the wait window

To make an event-driven validator PASS, induce a real fault in the wait
window. See [live-events-design.md](live-events-design.md)
for the full list of injection options.

```bash
# Terminal 2 — start the wait (e.g. RAS), longer timeout
sudo env XPU_SMI_BIN=<xpu-smi-or-xpumcli> <venv-python> \
    -m e2e_validation --live --only live_ras_events --live-timeout 300 \
    --sock-dir /run/xpumd --sock-name intelxpuinfo.sock

# Terminal 3 — trigger the real fault (KMD/L0 injection, TBD with KMD team)
#   e.g. PCIe AER / ACPI EINJ error inject, forced reset, thermal load, etc.
#   (AER = PCIe Advanced Error Reporting; EINJ = ACPI Error Injection Table.)
```
If a real `RAS_*` event reaches the stream before the timeout → **PASS**.

---

## 7. "Both" (offline + live)

Two commands, in order:

```bash
# 1) offline (policy logic)
XPU_SMI_BIN=<xpu-smi-or-xpumcli> python -m e2e_validation

# 2) live (real-event path) — daemon must be running
sudo env XPU_SMI_BIN=<xpu-smi-or-xpumcli> <venv-python> \
    -m e2e_validation --live \
    --sock-dir /run/xpumd --sock-name intelxpuinfo.sock
```

---

## CLI / env reference

| Flag | Meaning |
|------|---------|
| *(none)* | Offline mode — 9 synthetic validators |
| `--live` | Live mode — 10 real-event validators only (needs xpumd + xpu-smi) |
| `--live-timeout SECONDS` | Event wait before FAIL (default 300; `--live` only) |
| `--only NAME [NAME ...]` | Substring filter; both modes |
| `--sock-dir DIR` / `--sock-name NAME` | gRPC socket location |
| `-v`, `--verbose` | Debug logging |

| Env var | Purpose |
|---------|---------|
| `XPU_SMI_BIN` | xpu-smi binary (use `xpumcli` if that is what the box ships) |
| `E2E_LIVE_EVENT_TIMEOUT` | Same as `--live-timeout` |
| `E2E_SOCK_DIR` / `E2E_SOCK_NAME` | Same as `--sock-dir`/`--sock-name` |
| `E2E_LOG_DIR`, `E2E_VERBOSE` | Log dir / verbose |

## Exit codes

| Code | Meaning |
|------|---------|
| `0` | All validators passed |
| `1` | One or more FAIL/ERROR, or `--only` matched nothing |
| `2` | `--live` setup failed — xpu-smi missing, or socket connect timed out |

## Gotchas recap
- `sudo python` uses **system** Python (no `grpc`) → use the venv python by
  full path under sudo.
- Missing `XPU_SMI_BIN` under `--live` → aborts with exit 2.
- Socket is `root:root` 0660 → run as root or `chown` it to your group.
- Event-driven validators FAIL on healthy hardware with no fault — expected;
  they need a real fault/injection.
