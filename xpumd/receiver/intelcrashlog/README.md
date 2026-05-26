# Intel Crashlog Receiver

## Overview

The Intel Crashlog Receiver watches a directory for new GPU crash dump files
and emits each file content as a log record.

See the [Crash Log Framework project](https://github.com/intel/crashlog) for more details.

> [!CAUTION]
> Crashlog files must be created atomically (e.g. by writing to a temporary
> file and then renaming it) to avoid the receiver processing an incomplete
> file.

## Configuration

### Example Configuration

```yaml
receivers:
  intel_crashlog:
    directory: /var/log/crashlog
    glob: "*.bin"
    ignore_older_than: 10m
    add_attributes:
      hw.vendor: ACME
```

### Configuration Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `directory` | string | *required* | Directory to watch for new crashlog files |
| `glob` | string | *required* | Filename glob pattern matched against the base name of each file,  use `*` to match all files |
| `ignore_older_than` | duration | `0s` | Ignore files whose modification time is older than `startup_time - ignore_older_than`. `0s` means only files modified after startup are processed |
| `add_attributes` | map[string]string | `{}` | Static key/value attributes added to every emitted log record |

## Log Record Format

Each crashlog file produces a single log record:

| Field | Value |
|-------|-------|
| `Timestamp` | File modification time |
| `ObservedTimestamp` | Time the receiver processed the file |
| `Body` | Raw file contents (bytes) |

### Attributes

| Attribute | Type | Description |
|-----------|------|-------------|
| `file.name` | string | Base name of the crashlog file |
| `pci.bdf` | string | PCI Bus:Device.Function address parsed from the filename (e.g. `0000:00:1e.2`). Omitted if no BDF is found in the filename |
