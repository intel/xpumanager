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
* Provides OpenTelemetry and Prometheus metrics endpoints
  * Prometheus metric names (`hw_*`) and attributes follow OTel conventions
  * No "destructive" GPU operations supported, only GPU metric / status queries
* Separate endpoint to provide GPU state information for Kubernetes DRA Intel GPU driver
* Extensive configuration support through YAML configuration file


#### Missing metrics

Compared to XPUM 1.x, XPUM 2.x lacks support for the following GPU metrics:

* EU active/stall/idle metrics - were disabled by default in 1.x
* Fabric (XeLink) metrics - not relevant for BMG
* RAS (error) counters - not relevant for BMG


### Summary

In short, only Prometheus endpoint is common between 1.x and 2.x versions.
