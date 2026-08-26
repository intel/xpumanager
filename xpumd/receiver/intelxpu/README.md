# Intel XPU Receiver

## Overview

The Intel XPU Receiver collects Intel GPU telemetry through the
[Level Zero Sysman API](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/PROG.html)
and emits it as:

* **Metrics** for GPU utilization, power, frequency, temperature, memory, PCIe
  and device health, see the [metrics documentation](sysman/documentation.md)
  for details. The [metric semantics](semantics.md) document the reasoning
  behind the used metric and attribute naming.
* **Log records** for the GPU events reported by the driver, and for the GPU
  *info logs*, currently CPER records reporting hardware errors detected by
  the GPU.

The available telemetry depends on the hardware, firmware, kernel and GPU driver
of the host, and on the privileges of the daemon. See the
[XPUM daemon README](../../README.md#metrics) for the details.

## Configuration

### Example Configuration

```yaml
receivers:
  intel_xpu:
    collection_interval: 5s
    initial_delay: 1s
    timeout: 0
    sampling_interval: 1s
    fail_on_sysman_init_error: false
    info_logs:
      enabled: false
      instance_name: xpumd
    metrics:
      hw.memory.size:
        enabled: false
```

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `collection_interval` | duration | `1m` | Metrics collection interval, must be at least 1s and at least twice the `sampling_interval` |
| `initial_delay` | duration | `1s` | Delay before the first metrics collection, any non-positive value means immediately |
| `timeout` | duration | `0s` | Metrics collection timeout, `0s` means no timeout |
| `sampling_interval` | duration | `1s` | Sampling interval of the high-frequency (aggregated) metrics, must be at least 1ms |
| `fail_on_sysman_init_error` | bool | `false` | Whether to fail the daemon startup if the Sysman API cannot be initialized, e.g. when there is no GPU driver or no access to the GPU devices |
| `info_logs` | object | see below | Collection of the GPU info logs |
| `metrics` | map | `{}` | Per-metric `enabled` overrides, see the [metrics documentation](sysman/documentation.md) |

#### info_logs

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `enabled` | bool | `false` | Whether the info logs are collected |
| `instance_name` | string | `xpumd` | Name of the collection instance the records are collected into (a tracefs instance in the case of the CPER records). Required, and must not contain `/` |

## GPU Events

The receiver registers for all
[Sysman events](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html#zes-event-type-flags-t)
of every enumerated device and emits each reported event as a log record:

| Field | Value |
|-------|-------|
| `EventName` | `com.intel.gpu.` + event type, e.g. `com.intel.gpu.DEVICE_DETACH` |
| `Body` | Event type, e.g. `DEVICE_DETACH` |
| `Timestamp` | Time the event was received |
| `SeverityNumber`, `SeverityText` | Severity of the event, e.g. `Error` for the `DEVICE_DETACH` event |

A `DEVICE_ATTACH` event also triggers a full rescan of the device.

### Attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `hw.id` | string | Device UUID |
| `hw.name` | string | Device name, e.g. `gpu-1` |
| `hw.model` | string | Device model name |
| `pci.bdf` | string | PCI Bus:Device.Function address of the device |
| `pci.device_id` | string | PCI device ID |
| `pci.vendor_id` | string | PCI vendor ID |

## GPU Info Logs

Info logs are a family of log records that the GPU driver collects and provides
through the Sysman API. Currently the only supported format is
[CPER](https://uefi.org/specs/UEFI/2.10/Apx_N_Common_Platform_Error_Record.html)
(UEFI Common Platform Error Record), i.e. records of hardware errors detected
by the GPU. The records are emitted as opaque binary blobs. Tooling for
decoding the CPER sections is separate from XPUMD.

Collection is event driven. The records are read on the
`ZES_INTEL_CPER_DATA_AVAILABLE` Sysman event.

Enabling requires elevated privileges: the CPER records need read/write access
to the kernel tracing filesystem (`/sys/kernel/tracing`).

### Log Record Format

| Field | Value |
|-------|-------|
| `EventName` | `com.intel.gpu.info_log.cper` |
| `Body` | Raw CPER record (bytes) |
| `Timestamp`, `ObservedTimestamp` | Time the record was read, see the limitations below |
| `SeverityNumber`, `SeverityText` | `Error` |

#### Attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `pci.bdf` | string | PCI Bus:Device.Function address from the record metadata |
| `cper.platform_id` | string | Platform UUID from the record metadata |
| `cper.timestamp_us` | int | Record timestamp from the record metadata, microseconds since boot |
| `hw.id`, `hw.name`, `hw.model`, `pci.device_id`, `pci.vendor_id` | string | Attributes of the device matching `pci.bdf`, omitted if no device matches |

### Known Limitations

1. Severity is not part of the record metadata, so every record is emitted with
   `Error` severity.
2. The metadata timestamp is microseconds since boot, so it cannot reliably be
   converted to wall clock time. It is provided as an attribute only and the
   record is stamped with the observation time.
3. The event is the only trigger, i.e. there is no periodic polling.
   Records that are already pending when the daemon starts are emitted only when
   the next record arrives, and the records pending at shutdown are left in the
   buffer of the driver.
4. The triggering event of 3. is part of a separate Sysman extension, so the
   records are collected only if the driver implements both the info log and
   the driver scoped event extensions.
5. Collection is enabled once at startup. If enabling fails or a read fails later on,
   the records of that info log are not collected for the lifetime of daemon.
