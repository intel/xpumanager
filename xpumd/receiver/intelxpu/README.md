# Intel XPU Receiver

## Overview

The Intel XPU Receiver collects Intel GPU telemetry through the
[Level Zero Sysman API](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/PROG.html)
and emits it as:

* **Metrics** for GPU utilization, power, frequency, temperature, memory, PCIe
  and device health, see the [metrics documentation](sysman/documentation.md)
  for details. The [metric semantics](semantics.md) document the reasoning
  behind the used metric and attribute naming.
* **Log records** for the GPU events reported by the driver.

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
| `metrics` | map | `{}` | Per-metric `enabled` overrides, see the [metrics documentation](sysman/documentation.md) |

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
