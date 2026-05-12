# HW metric semantics

Contents:

* [Introduction](#introduction)
* [The problem](#the-problem)
* [Solution alternatives](#solution-alternatives)
  * [Hardware status alternatives](#hardware-status-alternatives)
  * [Metric namespacing alternatives](#metric-namespacing-alternatives)
  * [Linking alternatives](#linking-alternatives)
    * [Alternative 1](#alternative-1)
    * [Alternative 2](#alternative-2)
    * [Alternative 3](#alternative-3)
* [Current Intel GPU metrics](#current-intel-gpu-metrics)
  * [Overall approach](#overall-approach)
  * [Details](#details)
* [Other OTel HW semantics users][#other-otel-hw-semantics-users]
* [Related upstream issues](#related-upstream-issues)


## Introduction

HW metric semantics are simple.  There are:

* Metrics types, with values
* Hardware components, with attributes
* Relations between those
  * component <- metric
  * component <- subcomponent


## The problem

Current OTel HW metric semantics, and to some extent also semantics of other OTel metrics:
https://opentelemetry.io/docs/specs/semconv/hardware/

Are not suitable for providing GPU metrics because those semantics are...


_Too incomplete_

HW metric semantics are missing half of the relevant GPU metric types e.g. compared to ones listed in the Level-Zero oneAPI spec:
https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html


_Ambiguous & confusing as examples_

Most of the spec issues stem from HW metrics being separated into three categories:

* Common metrics, e.g. `hw.energy` & `hw.errors`: https://opentelemetry.io/docs/specs/semconv/hardware/common/
* "Semi-common" ones, e.g. `hw.temperature`: https://opentelemetry.io/docs/specs/semconv/hardware/temperature/
* `hw.<component>.<metric>` ones, e.g. `hw.gpu.io`: https://opentelemetry.io/docs/specs/semconv/hardware/gpu/

Arguments in the tickets for that separation are based on specific subset of metric types having associated HW
that directly provides the given metric values, e.g. in:
https://github.com/open-telemetry/semantic-conventions/issues/2438#issuecomment-3803582954

However, that argument & distinction does not generalize across
platforms. It is platform and hardware (HW) specific whether values
for given metric type come from specific HW, its firmware (FW), kernel
or user-space driver.

Different metric values of given type, even for the same HW, can also
come from different parts of the HW / FW / driver stack, e.g.
frequency request is done by a kernel driver, enforced by HW PMU unit
(based on TDP power limit), and reported in a HW register. That has no
semantically relevant difference to a value from a temperature sensor
(`hw.temperature`), or voltage regulator (`hw.voltage`).


_Insufficient for meaningful aggregation_

Another argument for the common metrics is aggregation of things like
errors and power usage.  However, different error metrics may be on a
completely different scale, and power usage is reported for components
at different levels of the same HW (e.g. GPU memory, package, whole card,
and the whole device PSU), so their aggregation will just mislead.

As to `hw.status{hw.state=...}` state metrics (having values 0 and 1),
summing them could be useful, but only if good and bad states can be
differentiated from each other, and currently specification does not
state how this should be done.


_Use inconsistent namespacing & linking_

GPU memory info needs to be queried from multiple HW namespaces:

* `hw.energy{hw.type="memory"}`
* `hw.errors{hw.type="memory"}`
* `hw.fan{hw.sensor_location="memory"}`
* `hw.gpu.memory.usage`
* `hw.gpu.memory.limit`
* `hw.gpu.memory.utilization`
* `hw.memory.size`
* `hw.power{hw.type="memory"}`
* `hw.status{hw.type="memory"}`
* `hw.temperature{hw.sensor_location="memory"}`

Instead of them being just under `hw.gpu[.memory]` namespace.

And e.g. GPU PCI, memory, RDMA bandwidth probably from:

* `hw.network.bandwidth.limit` (GPU RDMA BW?)
* `hw.network.bandwidth.utilization`
* `hw.network.io` (GPU RDMA BW?)
* `hw.gpu.io` (GPU PCI BW?)
* etc.

Instead of bandwidth metrics being namespaced consistently e.g. under `hw.gpu.memory`, `hw.gpu.pci`, `hw.gpu.rdma`.

Above `hw.<metric-type>` metrics require information about parent item, type and location, whereas `hw.<component>.<metric-type>` metrics need less of those.

There's also no specific HW info metric type, that could act as parent when there are no metrics available from the parent component.


_Require tautological attribute(s)_

Spec requires `hw.type` attribute for following metric types to be identical to metric type:
`hw.battery`, `hw.cpu`, `hw.disk_controller`, `hw.fan`, `hw.enclosure`, `hw.gpu`, `hw.logical_disk`, `hw.memory`,
`hw.network`, `hw.physical_disk`, `hw.power_supply`, `hw.tape_drive`, `hw.temperature`, `hw.voltage`.

Instead of it adding relevant information, e.g. to which component
fan, memory, network, temperature, voltage metrics belong to (CPU,
GPU...).


_Use inconsistent metric names_

* `system.cpu.frequency` <=> `hw.cpu.speed`
* `hw.memory.size` <=> `hw.gpu.memory.limit`
* `hw.energy` <=> `hw.host.energy` -- are `energy` (and `power`) common metrics or not?


_Seem just wrong_

* `hw.cpu.speed`: For HW, `frequency` is the unambiguous metric with an obvious base unit,
  whereas `speed` is often understood as operations per time, e.g. FLOPS.


_Do not differentiate between base and derived metrics_

Underlying HW and drivers provide base metrics like joules, usage and limits / sizes.

Derived metrics like `.watts` (joules / time) and `.utilization` (usage / limit) do not require HW themselves.
Such metrics could be provided by a generic metric processor for all hardware.

(This is a minor point.)


## Solution alternatives

Drop the artificial separation of common, "non-common" and `hw.<component>.<metric-type>` items, and use just:

* `hw.<component>[.subcomponent][.info]`: item providing component attributes, and knowledge that it exists
* `hw[.*].<metric-type>`: item providing metric values + their attributes for a component

And link all of them _in exactly the same way_.

Example HW component info metric:

```
hw.gpu.info{hw.id="gpu-1", hw.name="gpu-1-name", pci.bdf="00:0000:0000.1", ...} 1  # HW component
```

### Hardware status alternatives

For hardware status metrics to be meaningfully aggregatable, either:

* `hw.status` metric should be reported only for issues (not for "OK" states),
  * => if no `hw.status` metrics are reported, HW can be assumed to be in working condition
* Names for all "OK" state attributes should be explicitly specified in the spec, or
* There should be a separate attribute stating whether status metric is for an issue, or things being fine
  - (It could even be a scale for the state severity)


### Metric namespacing alternatives

Alternatives for metric value semantics are namespacing them under
metric type, or under component namespaces (OTel HW metrics spec is
mix of these):

|Unit|`<metric>{<attrib>="<component>"}` - alternative|`<component>.<metric>{}` - alternative|
|-----|-------|----------|
|---|**Bandwidth** (* = memory/pci/rdma/etc)|---|
|By|`hw.io{hw.type="*"}`|`hw.gpu.*.io`|
|By/s|`hw.io.limit{hw.type="*"}`|`hw.gpu.*.io.limit`|
|By/s|`hw.io.rate{hw.type="*"}`|`hw.gpu.*.io.rate`|
|1|`hw.io.utilization{hw.type="*"}`|`hw.gpu.*.io.utilization`|
||E.g.|`hw.gpu.memory.io.utilization`|
|---|**Energy** (for card/package/core/memory/etc)|---|
|J|`hw.energy{hw.type="gpu"}`|`hw.gpu.energy`|
|---|**Engine utilization** (for tile/partition/etc)|---|
|1|`hw.utilization{hw.type="gpu"}`|`hw.gpu.utilization`|
|---|**Error counters** (for tile/partition/etc)|---|
|1|`hw.errors{hw.type="gpu"}`|`hw.gpu.errors`|
|---|**Fan speed** (for tile/partition/memory/etc)|---|
|rpm|`hw.fan.speed`|`hw.gpu.fan.speed`|
|rpm|`hw.fan.speed.limit`|`hw.gpu.fan.speed.limit`|
|1|`hw.fan.speed_ratio`|`hw.gpu.fan.speed_ratio`|
|---|**Frequency** (for core/uncore/package/memory/etc)|---|
|Hz|`hw.frequency`|`hw.gpu.frequency`|
|Hz|`hw.frequency.request`|`hw.gpu.frequency.request`|
|Hz|`hw.frequency.limit`|`hw.gpu.frequency.limit`|
|1|`hw.frequency.utilization`|`hw.gpu.frequency.utilization`|
|---|**HW info / state** (* = any interface/sub-/component)|---|
|1|`hw.info{hw.type="*"}`|`hw.gpu[.*].info`|
|1|`hw.status{hw.type="*"}`|`hw.gpu[.*].status`|
|---|**Memory** (for tile/partition/etc)|---|
|By|`hw.memory.size{hw.type="gpu"}`|`hw.gpu.memory.size`|
|By|`hw.memory.usage{hw.type="gpu"}`|`hw.gpu.memory.usage`|
|By|`hw.memory.limit{hw.type="gpu"}`|`hw.gpu.memory.limit`|
|1|`hw.memory.utilization{hw.type="gpu"}`|`hw.gpu.memory.utilization`|
|---|**Power** (for card/package/core/memory/etc)|---|
|W|`hw.power{hw.type="gpu"}`|`hw.gpu.power`|
|W|`hw.power.limit{hw.type="gpu"}`|`hw.gpu.power.limit`|
|1|`hw.power.utilization{hw.type="gpu"}`|`hw.gpu.power.utilization`|
|---|**Power supply**|---|
|W|`hw.power_supply{hw.type="gpu"}`|`hw.gpu.power_supply`|
|---|**Temperature** (for GPU core/uncore/package/memory/etc)|---|
|Cel|`hw.temperature`|`hw.gpu.temperature`|
|Cel|`hw.temperature.limit`|`hw.gpu.temperature.limit`|
|1|`hw.temperature.utilization`|`hw.gpu.temperature.utilization`|

Notes:

* Derived `.rate` metrics are needed only when limits are unknown
* `hw.sensor_location` attribute tells actual metric source for the energy, fan, frequency, power and temperature metrics
* For `<metric>{<attribute>="*"}` items it would be useful to have standard attribute that tells directly
  (without JOIN query) that metric is for some GPU (sub-)component. This would greatly simplify queries
  for specific parent HW types (CPU / GPU / etc) e.g. in dashboards
* For "semi-common" metrics, OTel spec requires `hw.type` to match metric type,
  above use it for HW component type instead
* `[*.]hw.info` indicates presence of HW, its value could indicate whether interface is up
* Memory `.size` is HW property, `.limit` how much of it is usable (allocatable)
* Watts is energy usage within time interval, its limit would be for a pre-defined / fixed interval


### Linking alternatives

Information about the relations can be provided with formalized spec attributes, or informally through vendor attributes:

* device-2 <- subdevice-1 <=(link)= memory <=(link)= temperature-max
* device-2 <- temperature{subdevice=1, location=memory, type=max}


#### Alternative 1

All metrics are tied to some HW component through shared `hw.id` attribute, only their `hw.name` attributes differ:

```
hw.time{hw.id="gpu-1", hw.name="engine-1-compute", hw.task="compute", ...} <value>  # HW metric
hw.memory{hw.id="gpu-1", hw.name="memory-2", ...} <value>  # HW metric
hw.status{hw.id="gpu-1", hw.name="memory-2", hw.type="memory", hw.state="ok", ...} 1  # HW component status
```

If HW has significant enough subcomponents with multiple metric types, those components can have their own info item with their own `hw.id`,
and be linked to the parent item through `hw.parent` attribute. Their metrics will share the same `hw.id`, but do not include `hw.parent` attribute.


_subcomponent linking_

Example of gpu <- tile <- memory:

```
hw.gpu.tile.info{hw.id="gpu-1-tile-2", hw.parent="gpu-1"} 1  # HW sub-component
hw.memory.info{hw.id="gpu-1-tile-2-mem-1", hw.parent="gpu-1-tile-2", hw.memory.type="DDR6"} 1  # HW sub-sub-component
hw.memory.size{hw.id="gpu-1-tile-2-mem-1", hw.name="mem-1-name"} <size>  # its metric
```

NOTE: If there's no real reason for listing intermediate subcomponents (e.g. GPU tiles) with their parent linking,
metrics could also refer directly to GPU and just include additional e.g. `hw.subdevice_id` attribute:

```
hw.memory.info{hw.id="gpu-1", hw.subdevice_id="2", hw.memory.type="DDR6"} 1      # HW sub-component info
hw.memory.size{hw.id="gpu-1", hw.subdevice_id="2", hw.name="mem-1-name"} <size>  # its metric
```

TODO: Should sub-component info be instead under `hw.<component>` items, like this?

```
hw.gpu.memory.info{hw.id="gpu-1", hw.subdevice_id="2", hw.memory.type="DDR6"} 1  # HW sub-component info
hw.memory.size{hw.id="gpu-1", hw.subdevice_id="2", hw.name="mem-1-name"} <size>  # its metric
```


#### Alternative 2

All metrics are tied to some HW component through their `hw.parent` attribute, and both `hw.id` & `hw.name` attributes are unique to all items:

```
hw.time{hw.parent="gpu-1", hw.id="gpu-1-engine-1", hw.name="engine-1-compute", hw.task="compute", ...} <value>  # HW metric
hw.time{hw.parent="gpu-1", hw.id="gpu-1-engine-2", hw.name="engine-2-compute", hw.task="compute", ...} <value>  # HW metric
hw.status{hw.parent="gpu-1-engine-1", hw.id="gpu-1-engine-1-status", hw.state="busy", ...} 1  # HW component status
```


#### Alternative 3

All items have unique `hw.id` / `hw.name` and linkage is done through separate link items, like in databases:

```
hw.time{hw.id="gpu-1-engine-1", hw.name="engine-1-compute", hw.task="compute", ...} <value>
hw.link{hw.id="gpu-1-engine-1", hw.parent="gpu-1"} 1  # item linkage
hw.time{hw.id="gpu-1-engine-2", hw.name="engine-2-compute", hw.task="compute", ...} <value>
hw.link{hw.id="gpu-1-engine-2", hw.parent="gpu-1"} 1  # item linkage
```


## Current Intel GPU metrics

Intel _XPU Manager_ OTel exporter GPU metrics are based on information
provided by the Level-Zero Sysman API[^1], and the Intel HW + driver
stack below it.

### Overall approach

Current item linking uses [alternative 1](#alternative-1) for simplicity.

For now, exporter HW metric types and their attributes are mix of OTel
specified ones and Intel specific ones.

`hw.status` metrics:

* Only non-issue component states are `ok` and `unknown`
* Except for component `ok` and GPU `reset_needed` states, `hw.status` metrics
  are not exported until given (issue) state first becomes known & active, to
  significantly reduce number of metrics
  - Issues should be rare, and underlying HW + driver stack may support only
    small subset of the states specified in the Level-Zero Sysman API[^1]
* `unknown` states are reported only if corresponding component states were known earlier.


### Details

Used metrics names:

* common OTel spec metrics
* "semi-common" OTel spec metrics
* `hw.<component>` OTel spec metrics
* New GPU metrics for OTel `hw.*` namespace:
  *  `hw.frequency`, `hw.frequency.*` (all of them)
  * `hw.gpu.bandwidth.*`, `hw.gpu.info`, `hw.gpu.io.rate`
  * `hw.memory.bandwidth.*`, `hw.memory.io`, `hw.memory.io.rate`, `hw.memory.usage`
  * `hw.power.limit`

Used attributes:

* Common and other OTel spec HW attributes
* New Intel GPU attributes:
  * In `com.intel.*` vendor namespace:
    * Attribs for all metrics: `.subdevice_id`
    * Metric specific attribs: `.power.limit.*`, `.speed.throttle_reason`, `.subdevice_count`
  * In OTel `hw.*` namespace:
    * Metric specific attribs: `hw.frequency.domain`, `hw.gpu.type`, `hw.memory.*`, `hw.limit_type`
  * In OTel `error.*` namespace:
    * `error.category` attribute (for `hw.errors`)
  * In `pci.*` namespace:
    * Attribs for all metrics: `pci.bdf`
    * Device info/state attribs: `pci.device_id`, `pci.lanes`, `pci.link_gen`, `pci.vendor_id`
  * Not namespaced (aggregation attribs for metrics that need to be queried internally at higher frequency):
    * `aggregation`, `sample.status`, `statistic`

New values for the OTel spec HW attribute enumerations:

* `hw.*{hw.type=...}`:
  * `frequency`, `pci_link`
* `hw.status{hw.state=...}`:
  * GPU `reset_needed`, ECC states (5), PCI link states (up to 5), memory health states (up to 5),
    frequency `throttled` (with separate metric for up to 12 throttling reasons)

=> some of them conflict with the OTel spec guidance as they are not namespaced,
or use OTel `hw.*` namespace for metrics that are not in the spec yet.

Meaning that the whole set is likely to change in the future (based on discussions
with OTel upstream and other GPU vendors).

For more details, see [Sysman receiver metrics documentation](sysman/documentation.md),
and Level-Zero Sysman API spec[^1].


## Other OTel HW semantics users

In OpenLIT project, except for `hw.errors`, all metrics types are
under `hw.gpu` namespace, including ones that OTel spec has split to
"common" and "semi-common" categories:
https://github.com/openlit/openlit/tree/openlit-1.20.0/opentelemetry-gpu-collector#metrics

In the OTel collector contrib repository:
https://github.com/open-telemetry/opentelemetry-collector-contrib

Of the 113 receivers added there, only "Cisco OS" receiver uses HW
semantics, and even that uses _only_ `hw.type` attribute, nothing
else.

As to the other components in that project, "BMC Helix" exporter
README and tests refer to HW semantics, as do tests for "Prometheus"
translator package, but their actual code does not.

=> fixing HW semantics would affect only few projects, and not much.


## Related upstream issues

Some upstream tickets discussing issues with the current OTel spec HW semantics:

* Document common metric names and when they should be used:
  https://github.com/open-telemetry/semantic-conventions/issues/211
* Issues with Hardware Metrics semantic conventions:
  https://github.com/open-telemetry/semantic-conventions/issues/940
* Common vs. other metrics types + metric linkage:
  https://github.com/open-telemetry/semantic-conventions/issues/2918
* `hw.host.power`/`.energy` versus `hw.power`/`.energy` metrics:
  https://github.com/open-telemetry/semantic-conventions/issues/1055
* Need for enum type (e.g. for error categories):
  https://github.com/open-telemetry/opentelemetry-specification/issues/1711

[^1]: https://oneapi-src.github.io/level-zero-spec/level-zero/latest/sysman/api.html
