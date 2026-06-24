# Live (real-event) validation design

This note explains *why* `--live` was reworked to consume **only real**
KMD-sourced events, how the pieces fit together, and how to drive a real
fault so an event-driven validator can PASS.

---

## Motivation

The original `--live` mode reused the offline use-case validators, which
**synthesize** `DeviceEvent` objects and feed them straight into the policy
engine. That proves the *policy wiring* but says nothing about whether a
real fault originating in the kernel actually reaches the Python client.
A reviewer can pass `--live` on a healthy box, see all-green, and wrongly
conclude the end-to-end event path works.

This change makes `--live` validate the **real** path and nothing else:

```
KMD (i915/xe) → Level Zero sysman → xpumd → gRPC Unix socket → e2e_validation
```

If a real event does not arrive, the event-driven validators must **FAIL**
(not PASS, not SKIP). Green now means the real pipeline delivered an event.

---

## Two mutually-exclusive modes

| Mode | Flag | Validators | Events | Daemon |
|------|------|-----------|--------|--------|
| Offline | *(default)* | 9 synthetic/policy validators | fabricated | not needed |
| Live | `--live` | 10 live validators **only** | real stream only | required |

`--live` and offline are exclusive *by design* — there is no combined mode.
"Run both" means running the two commands back to back. This keeps the
"did the real path work?" signal from being diluted by synthetic passes.

---

## Live validators

**Snapshot validators** inspect the inventory/health the daemon always
reports, so they PASS immediately on healthy hardware:

- `device_discovery`, `device_info_fields`
- `health_stream`, `health_watcher`, `health_smi_crosscheck`

**Event-driven validators** attach to the live gRPC streams and wait for a
*real* matching event; they FAIL on timeout:

| Validator | Event types it waits for |
|-----------|--------------------------|
| `live_health_event` | `HEALTH_WARNING` / `HEALTH_CRITICAL` / `HEALTH_FAILED` |
| `live_pcode_error` | `PCODE_ERROR` / `DEVICE_WEDGED` |
| `live_ras_events` | `RAS_CORRECTABLE` / `RAS_UNCORRECTABLE` |
| `live_survivability` | `DEVICE_SURVIVABILITY` |
| `live_thermal_throttle` | `THERMAL_THROTTLE` |

### Two streams: health *and* events

The daemon exposes **two** server-streaming RPCs and the live validators
consume **both**:

- **`WatchDeviceHealth`** (`HealthWatcher`) — periodic per-domain health
  *status*. Source of the health-degradation transitions
  (`HEALTH_WARNING`/`CRITICAL`/`FAILED`).
- **`WatchDeviceEvents`** (`EventWatcher`) — *discrete* hardware events as
  they occur: survivability, RAS, device-reset-required (wedge), thermal,
  etc. This stream has **no replay** (only events after connect are
  delivered), so the session connects it *before* the wait window opens.

This split matters: the fault events most validators target
(survivability, RAS, reset/wedge) arrive on `WatchDeviceEvents`, **not** on
the health stream. The health stream only surfaces a derived
`gpu/reset_needed` *warning*. Consuming only `WatchDeviceHealth` (the
original design) made `live_survivability`/`live_ras_events`/
`live_pcode_error` FAIL even when the real fault occurred — confirmed on a
Battlemage (B60) box by injecting a CSC error via igt `xe_survivability`,
which produced a real `survivability_mode_detected` event on
`WatchDeviceEvents`. `EventWatcher` maps each event's `reason` (the
lowercased L0 sysman event-flag name, e.g. `survivability_mode_detected`)
to the corresponding `EventType`.

A device stuck in a fault mode re-emits the *same* event on every daemon
poll, so `EventWatcher` records the first of each distinct
`(type, bdf, reason)` signature and suppresses the duplicates — bounding
memory regardless of how long the fault persists.

### Shared stream session

All event-driven validators share one `LiveStreamSession`
(`validators/live_events.py`). It runs one background `HealthWatcher` and
one `EventWatcher` and arms one **shared deadline**, so the whole run is
bounded by a single `live_event_timeout_s` window rather than `N × timeout`.
The first event-driven validator blocks until the deadline; the rest
evaluate instantly against events collected (from both streams) during that
window.

The runner (`run.py`) executes the snapshot validators **first**, then
starts the session immediately before the event-driven validators, so the
full timeout window is available for real events (it is not consumed by the
~30s `health_watcher` snapshot check).

---

## Timeout

`live_event_timeout_s` defaults to **300s (5 min)**. Override per run:

- CLI: `--live-timeout SECONDS`
- env: `E2E_LIVE_EVENT_TIMEOUT=SECONDS`

A non-positive value is rejected at construction time (it would make every
event-driven validator FAIL instantly).

---

## Driving a real fault (so an event-driven validator PASSES)

On healthy hardware these fault events do not occur spontaneously — you
must induce a real fault during the wait window. Options, roughly in order
of how close they sit to the metal (exact mechanism is **TBD with the KMD
team**):

1. **PCIe AER / ACPI EINJ error injection** — inject a correctable or
   uncorrectable PCIe error so the KMD reports a RAS event. (`AER` =
   PCIe *Advanced Error Reporting*; `EINJ` = ACPI *Error Injection
   Table*. Both are standard fault-injection facilities, not typos.)
2. **Forced engine reset / device wedge** — provoke a Pcode/device-wedged
   path through the KMD.
3. **Thermal load** — drive the part hot enough to trip a thermal-throttle
   health transition.
4. **L0 sysman / KMD test hooks** — if available, use a debug interface to
   raise the corresponding health domain to WARNING/CRITICAL.

Whatever the mechanism, the success criterion is the same: a real
`PCODE_ERROR` / `RAS_*` / `DEVICE_SURVIVABILITY` / `THERMAL_THROTTLE` /
health-degradation event reaches the gRPC stream before the timeout, at
which point the matching validator reports **PASS**.

---

## See also

- `e2e_validation/docs/QUICKSTART.md` — how to run every scenario.
- `e2e_validation/README.md` — framework overview and use-case catalogue.
