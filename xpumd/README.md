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

[Changes](docs/CHANGES.md) lists differences in corresponding functionality compared to XPUM v1.x.


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
            subgraph RECV["Receivers"]
                RECV_XPU["Intel XPU Receiver<br/>(intelxpu)"]
                RECV_LOG["Intel Crashlog<br/>(intel_crashlog)"]
            end

            subgraph PROC["Processor pipeline"]
                PROC_XPU["Status Processor<br/>(intelxpustatus)"]
                %% PROC_OTHERS["Standard Processors"] %%
            end

            RECV -->|metrics| PROC_XPU
            %% PROC_XPU --> PROC_OTHERS %%

            subgraph EXP["Exporters"]
                EXP_INFO["Device Info Exporter<br/>(intelxpuinfo)"]
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
    style PROC_XPU fill:#4a90e2,stroke:#2e5c8a,stroke-width:2px,color:#fff
    style EXP_INFO fill:#4a90e2,stroke:#2e5c8a,stroke-width:2px,color:#fff
```


## Deployment

### Standalone

Run XPUM daemon with its example config (with `$TAG` being the desired release tag):

```bash
docker run -it --rm --user 0 --cap-drop ALL --cap-add SYS_ADMIN \
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

![GPU metrics dashboard](docs/xpumd-dashboard.png)


## Features

### Metrics

See the [`intelxpu` receiver documentation](receiver/intelxpu/sysman/documentation.md)
for the list of supported GPU metrics and attributes.

Metrics availability depends on the underlying host hardware,
firmware, kernel and its GPU driver version, and the user-space
Level-Zero driver (included in the container image).

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
* SYS_ADMIN capability:
  - Docker options: `--cap-add SYS_ADMIN`
  - Adds PMU metrics: GPU engine utilization
* Access to MEI devices:
  - Docker options: `--device /dev/mei<idx>`
  - Required for information on subset of the firmware types

### Device info exporter

The XPUM daemon implements a custom exporter that exposes GPU capability and health information.
It serves a custom gRPC API at local Unix socket (`/run/xpumd/intelxpuinfo.sock` by default).

The device info exporter is enabled by the default configuration file
([`config-example.yaml`](config-example.yaml)) and the [Helm chart](charts/xpumd/README.md).


## Development

See [DEVELOPMENT.md](docs/DEVELOPMENT.md) for instructions on how to build, run and test the XPUM daemon.
