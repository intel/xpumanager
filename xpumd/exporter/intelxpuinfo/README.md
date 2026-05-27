# Intel XPU Info Exporter

## Overview

The Intel XPU Info Exporter translates metrics and logs into device information
that is exposed via a [`gRPC API`](api/deviceinfo/v1alpha1/).

### Metrics pipeline

The exporter consumes three metrics:

| Metric | Description |
|--------|-------------|
| `hw.gpu.info` | Device identity (model, PCI info, firmware versions etc.) |
| `hw.memory.size` | Device memory properties |
| `hw.status` | Per-component health status, see `hw_status_mappings` in config |

Connected clients receive a stream of health, identification and configuration
information for all known XPU devices.

### Logs pipeline

The exporter also consumes events (log records) produced by the
[intelxpu](../receiver/README.md) receiver. Events with the `hw.id`
attribute are translated into device events.

## Configuration

### Example Configuration

```yaml
exporters:
  intelxpuinfo:
    hw_status_mappings:
      # Temperature health domain
      - filters:
          - key: "hw.type"
            values: ["temperature"]
        # Combine hw.type and hw.sensor.location attributes, like "temperature.memory"
        health_domain: "{{.hw_type}}.{{.hw_sensor_location}}"
        state_mapping:
          # hw.state "ok" maps to severity "ok". Reason and message default to "ok" and "" respectively.
          "ok":
            severity: "ok"
          # hw.state "warning" maps to severity "warning" and reason "HighTemperature", message defaults to "".
          "warning":
            severity: "warning"
            reason: "HighTemperature"
          # hw.state "critical" maps to severity "critical", with specifuc reason and message.
          "critical":
            severity: "critical"
            reason: "CriticalTemperature"
            message: "Temperature critical, thermal throttling"

      # Catch-all mapping for other hw.status metrics
      - health_domain: "{{.hw_type}}"
        state_mapping:
          "ok":
            severity: "ok"
          "*":
            severity: "unknown"
```

Mappings are evaluated in order; the first matching entry (`filters` match the
metric attributes) is used. Place more specific mappings before less specific
ones and a possible catch-all (empty `filters`) last.

### Configuration Parameters

#### Top-level

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `endpoint` | string | `$XDG_RUNTIME_DIR/intelxpuinfo.sock` | Listen endpoint for the gRPC server, only unix domain sockets are supported |
| `hw_status_mappings` | []HwStatusMapping | *required* | Ordered list of mappings from `hw.status` device health information. The first matching entry is used |

In addition to `endpoint`, the exporter supports a wide range of gRPC
configuration options not listed above. See the OTel collector
[gRPC configuration settings](https://github.com/open-telemetry/opentelemetry-collector/blob/main/config/configgrpc/README.md)
documentation for details.

#### HwStatusMapping

| Parameter | Type | Default | Description |
|-----------|------|-------- | -------------|
| `filters` | []AttributeFilter | [] | Attribute filters that must all match for this mapping to apply (AND semantics). An empty list matches unconditionally and acts as a catch-all |
| `health_domain` | string | *required* | [Go template](https://pkg.go.dev/text/template) for the health domain name. Attribute keys are available as template variables with dots replaced by underscores (e.g. `hw.type` -> `.hw_type`) |
| `state_mapping` | map[string]HwStateMapping | {} | Maps `hw.state` attribute values to health status output. The special key `"*"` is a a catch-all for any state not matched by an exact key. Leaving this empty can be used to filter out certain metrics (not be picked by a catch-all mapping) |
| `show_all_states` | bool | `false` | When `true`, all active `hw.state` values are reported as separate health status entries instead of only the one with the highest severity for each health domain. Useful when multiple concurrent issues should all be visible |

#### AttributeFilter

| Parameter | Type | Description |
|-----------|------|-------------|
| `key` | string | Attribute key to filter on |
| `values` | []string | Attribute values to match (logical OR) |

#### HwStateMapping

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `severity` | string | *required* | Resulting severity level. Valid values: `unknown`, `ok`, `warning`, `critical`, `failed` |
| `reason` | string | `hw.state` attribute value | Short single-word reason included in the health status |
| `message` | string | `""` | Optional human-readable detail message |

## gRPC API

See [`api/deviceinfo/v1alpha1/`](api/deviceinfo/v1alpha1/) for the API definitions.

There's a small command-line client [`xpuinfo-cli/`](xpuinfo-cli/) for verifying
the exporter output during development and testing.
