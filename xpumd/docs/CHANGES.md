# XPUM daemon user visible changes

## XPUM 2.1.0

* XPUMD config: Module name changes for readability / consistency
  * `intelxpu` => `intel_xpu`
  * `intelxpuinfo` => `intel_xpu_info` (also default name for gRPC local socket changed)
  * `intelxpustatus` => `intel_xpu_status`
* XPUMD init: Do not fail if `intel_xpu` L0 Sysman init fails (fixes [#129](https://github.com/intel/xpumanager/issues/129))
  * Avoids DRA driver exit when it's configured to use XPUMD for GPU health info
  * Allows `intel_crashlog` module to be used even when L0 driver stack finds no GPUs
  * Re-initializes Sysman after getting device detach / attach event
  * (`fail_on_sysman_init_error` config option restores earlier behavior)
* Sysman Go bindings: Update to L0 API spec v1.16.24
* XPUMD image: L0 driver stack update to 26.27.39122.11
* Go dependencies: Security enhancements + other version updates (to gRPC, OTel etc modules)


## XPUM 2.0.1

* Default config changes:
  * Filter out / avoid `hw.status` warnings for ECC states that are not indicative of health
  * Disable backtraces for error / warning logs (as they seem to be interpreted as crashes)
* Metric output fixes:
  * Filter out bogus power values caused by Sysman energy counter value wraparounds
    (fixes [#130](https://github.com/intel/xpumanager/issues/130))
* XPUMD API: New `intelxpuinfo/api` Go module for the GPU info gRPC endpoint
* XPUMD image: L0 driver stack update to 26.22.38646.4
* XPUMD code / Go dependencies: Security enhancements

XPUMD integration with other Kubernetes GPU components:

* DRA GPU driver can use XPUMD for node GPU health information, and to
  avoid needing privileged mode itself
* GPU operator supports installing XPUMD along with the DRA driver


## XPUM v2.0 vs XPUM v1.x

Below is an overview of the major differences between the XPUM 1.x and
2.0 versions, for their daemon / exporter / remote API functionality.

### XPUM 2.x rewrite:

* Daemon and exporter in the same process
  * Implemented in Go on top of OpenTelemetry (OTel) packages and Level-Zero Go bindings
* Provides OpenTelemetry and Prometheus metrics exporting endpoints
  * Prometheus metric names (`hw_*`) and attributes follow OTel conventions
  * No "destructive" GPU operations supported, only GPU metric / status queries
  * OpenTelemetry endpoint also provides events and logs in addition to metrics
* Separate endpoint to provide GPU state information for Kubernetes DRA Intel GPU driver
* Extensive configuration support through YAML configuration file
* Kubernetes deployment via GHCR Helm repository `xpumd` chart

#### Missing metrics

Compared to XPUM 1.x, XPUM 2.0 lacks support for the following GPU metrics:

* EU active/stall/idle metrics
  * OA HW counter config is GPU-wide, and processing them can have noticeable
    performance impact, so these were not enabled by default even in v1.x
  * Use instead e.g. VTune for debugging performance issues
* Fabric (XeLink) metrics - not relevant for BMG

### XPUM 1.x:

* XPUM daemon and exporter functionality in separate processes
  * Daemon implemented in C++, on top of `libxpum`
  * Exporter implemented in Python, on top of `gunicorn` server and the XPUM daemon
* Daemon provides additional functionality for `xpumcli` (compared to daemon-less `xpu-smi`)
* Exporter provides XPUM REST API and Prometheus metrics endpoints
  * Prometheus metric names and attributes are XPUM specific (`xpum_*`)
  * XPUM REST API endpoint supports all of XPUM functionality,
    including "destructive" GPU operations
  * Endpoints configured through env vars and CLI options
  * Metrics need to be configured on the daemon side
    (options use numeric metric IDs instead of metric names)
* Kubernetes deployment via project `kustomize` files
