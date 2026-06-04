# XPUM 1.x vs 2.x

This document describes user visible differences between the XPUM 1.x and 2.x versions,
for their daemon / exporter / remote API functionality.

## Major differences

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


### XPUM 2.x rewrite:

* Daemon and exporter in the same process
  * Implemented in Go on top of OpenTelemetry (OTel) packages and Level-Zero Go bindings
* Provides OpenTelemetry and Prometheus metrics exporting endpoints
  * Prometheus metric names (`hw_*`) and attributes follow OTel conventions
  * No "destructive" GPU operations supported, only GPU metric / status queries
  * OpenTelemetry endpoint also provides events and logs in addition to metrics
* Separate endpoint to provide GPU state information for Kubernetes DRA Intel GPU driver
* Extensive configuration support through YAML configuration file


#### Missing metrics

Compared to XPUM 1.x, XPUM 2.0 lacks support for the following GPU metrics:

* EU active/stall/idle metrics
  * OA HW counter config is GPU-wide, and processing them can have noticeable
    performance impact, so these were not enabled by default even in v1.x
  * Use instead e.g. VTune for debugging performance issues
* Fabric (XeLink) metrics - not relevant for BMG


### Summary

In short, only Prometheus endpoint is common between 1.x and 2.x versions.
