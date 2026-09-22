# Intel(R) XPU Manager (XPUM) Daemon

- [Introduction](#introduction)
- [Architecture](#architecture)
- [Deployment](#deployment)
  - [Standalone](#standalone)
  - [Kubernetes](#kubernetes)
  - [Grafana dashboard](#grafana-dashboard)
- [Features](#features)
  - [Metrics](#metrics)
  - [Device info exporter](#device-info-exporter)
- [Development](#development)


## Introduction

XPUM (v2.x) daemon is a custom
[OpenTelemetry Collector](https://opentelemetry.io/docs/collector/) that
provides:

* Intel GPU metric exporters
* GPU status information for Kubernetes Intel GPU resource drivers

[Changes](docs/CHANGES.md) lists daemon differences from earlier XPUM versions.


## Architecture

```mermaid
graph TB
    subgraph LEGEND
        LEGEND_PROJECT["Project components"]
        LEGEND_EXTERNAL["External components"]
    end

    subgraph CONT["Container Image"]
        SYSTEM_LIBRARIES["System Libraries<br/>[level-zero, glibc...]"]

        subgraph XPUMD["XPUM Daemon - xpumd"]
            subgraph EXT["Extensions"]
                EXT_WATCH["Device Watch<br/>(intel_device_watch)"]
            end

            subgraph RECV["Receivers"]
                RECV_XPU["Intel XPU Receiver<br/>(intel_xpu)"]
                RECV_LOG["Intel Crashlog<br/>(intel_crashlog)"]
            end

            subgraph PROC["Processor pipeline"]
                PROC_XPU["Status Processor<br/>(intel_xpu_status)"]
                %% PROC_OTHERS["Standard Processors"] %%
            end

            RECV -->|metrics| PROC_XPU
            %% PROC_XPU --> PROC_OTHERS %%

            subgraph EXP["Exporters"]
                EXP_INFO["Device Info Exporter<br/>(intel_xpu_info)"]
                EXP_PROM["Prometheus Exporter"]
                EXP_OTEL["OpenTelemetry Exporter"]
            end
            %% change PROC_XPU => PROC_OTHERS if other processors added %%
            PROC_XPU -->|processed metrics| EXP_PROM
            PROC_XPU -->|GPU capabilities / status| EXP_INFO
            PROC_XPU -->|processed metrics / events| EXP_OTEL
            RECV_LOG -->|crash logs| EXP_OTEL
        end
    end

    subgraph TELEM["Telemetry Stack"]
        PROM["Prometheus Server"]
        OTEL_COL["OpenTelemetry Collector"]
        GRAFANA["Grafana/Visualization"]
    end

    subgraph K8S["Kubernetes Integration"]
        DRA["DRA Driver<br/>Intel GPU Resource Driver"]
        SCHEDULER["Kubernetes Scheduler"]
    end

    SYSTEM_LIBRARIES --> RECV_XPU

    EXP_PROM -->|"HTTP(S)"| PROM
    EXP_OTEL -->|"OTLP<br/>gRPC/HTTP(S)"| OTEL_COL
    PROM --> GRAFANA
    OTEL_COL --> GRAFANA

    EXP_INFO -->|"gRPC<br/>Unix Socket"| DRA
    DRA --> SCHEDULER

    style LEGEND_PROJECT fill:#4a90e2,stroke:#2e5c8a,stroke-width:2px,color:#fff
    style LEGEND_EXTERNAL fill:#e1f5ff
    style SYSTEM_LIBRARIES fill:#e1f5ff
    style XPUMD fill:#e1f5ff
    %% style PROC_OTHERS fill:#e1f5ff %%
    style EXP_PROM fill:#e1f5ff
    style EXP_OTEL fill:#e1f5ff
    style CONT fill:#fff3e0
    style K8S fill:#e8f5e9
    style TELEM fill:#e8f5e9
    style RECV_XPU fill:#4a90e2,stroke:#2e5c8a,stroke-width:2px,color:#fff
    style RECV_LOG fill:#4a90e2,stroke:#2e5c8a,stroke-width:2px,color:#fff
    style PROC_XPU fill:#4a90e2,stroke:#2e5c8a,stroke-width:2px,color:#fff
    style EXP_INFO fill:#4a90e2,stroke:#2e5c8a,stroke-width:2px,color:#fff
    style EXT_WATCH fill:#4a90e2,stroke:#2e5c8a,stroke-width:2px,color:#fff
```


## Deployment

### Standalone

Run XPUM daemon with its example config (with `$TAG` being the desired release tag):

```bash
docker run -it --rm --user 0 --cap-drop ALL --cap-add PERFMON \
  --device /dev/dri --publish 8080:8080 ghcr.io/intel/xpumanager/xpumd:$TAG \
  --config /etc/xpumd/config-example.yaml
```

For integration-style runs without a real Level Zero userspace driver, the
image also ships a stub driver. See
[`DEVELOPMENT.md`](docs/DEVELOPMENT.md#testing-container-image-with-stub-driver)
for detailed instructions.

See also [Testing container image](docs/DEVELOPMENT.md#testing-container-image).

### Kubernetes

See the [Helm chart](charts/xpumd/README.md) for deployment instructions.

For an example of a more complete telemetry stack, see either:

* [MONITORING.md](docs/MONITORING.md) for using XPUM daemon with Prometheus + Grafana, or
* [OTEL_STACK.md](docs/OTEL_STACK.md) for deploying XPUM daemon with an OpenTelemetry collector backend

### Grafana dashboard

Helm chart installs Grafana dashboard, but one can also load manually
[dashboard JSON version](charts/xpumd/json/) to Grafana.

![GPU metrics dashboard (top)](docs/xpumd-dashboard-1.png)
...
![GPU metrics dashboard (middle)](docs/xpumd-dashboard-2.png)


## Features

### Metrics

See the [`intel_xpu` receiver documentation](receiver/intelxpu/sysman/documentation.md)
for the list of supported GPU metrics and attributes.

Metrics availability depends on the underlying host hardware,
firmware, kernel and its GPU driver version, and the user-space
Level-Zero driver (included in the container image, [driver release
notes](https://github.com/intel/compute-runtime/releases)).

That set is further constrained by the host kernel, depending on the
privileges given to the (XPUM daemon) container / process querying the
metrics:

* Writable GPU device files:
  - Docker base options: `--device /dev/dri --cap-drop ALL`
  - Needed for all metrics
* User/group can write to GPU device files:
  - Docker options: `--user 65534:$(awk -F: '/render/{print $3}' /etc/group)`
  - Metrics: power, frequency, memory usage
* User 0:
  - Docker options: `--user 0`
  - Adds metrics: temperature, memory + PCIe bandwidth
* PERFMON (or SYS_ADMIN) capability:
  - Docker options: `--cap-add PERFMON`
  - Adds PMU metrics: GPU engine utilization
* Access to MEI devices:
  - Docker options: `$(for dev in /dev/mei[0-9]*; do [ -e "$dev" ] && echo "--device $dev"; done)`
  - Required for information on subset of the firmware types

> [!NOTE]
> If GPU utilization metrics are missing with the user `0` and the `PERFMON` capability,
> or container fails to start, underlying system (kernel or container engine) may be too
> old to support `PERFMON` capability. This can be worked around by using the (much wider)
> SYS_ADMIN capability instead (`--cap-add SYS_ADMIN`).

### Device info exporter

The XPUM daemon implements a custom exporter that exposes GPU capability and health information.
It serves a custom gRPC API at local Unix socket (`/run/xpumd/intel_xpu_info.sock` by default).

The device info exporter is enabled by the default configuration file
([`config-example.yaml`](config-example.yaml)) and the [Helm chart](charts/xpumd/README.md).

### Device watch

GPUs that appear on the node after the container started are not usable by it.
The device watch extension periodically compares sysfs against the device files
and reports devices that are missing, inaccessible or stale. It can also shut
down the collector so that the devices are re-enumerated on restart.

See the [`intel_device_watch` extension documentation](extension/inteldevicewatch/README.md)
for detailed configuration and usage instructions.


## Development

See [DEVELOPMENT.md](docs/DEVELOPMENT.md) for instructions on how to build, run and test the XPUM daemon.
