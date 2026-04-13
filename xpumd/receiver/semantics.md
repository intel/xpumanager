# HW metric semantics

Contents:

* [Introduction](#introduction)
* [The problem](#the-problem)
* [The solution](#the-solution)
  * [Linking alternatives](#linking-alternatives)
    * [Alternative 1](#alternative-1)
    * [Alternative 2](#alternative-2)
    * [Alternative 3](#alternative-3)
* [Current Intel GPU metrics](#current-intel-gpu-metrics)
* [References](#references)


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

In the spec, HW metrics are separated into three categories:

* Common metrics, e.g. `hw.energy` & `hw.errors`: https://opentelemetry.io/docs/specs/semconv/hardware/common/
* "Semi-common" ones, e.g. `hw.temperature`: https://opentelemetry.io/docs/specs/semconv/hardware/temperature/
* `hw.<component>.<metric>` ones, e.g. `hw.gpu.io`: https://opentelemetry.io/docs/specs/semconv/hardware/gpu/

Arguments in the tickets for that separation are based on specific subset of metric types having separate HW
for measuring the given metric values, e.g. in:
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

_Inconsistent_

* `system.cpu.frequency` <=> `hw.cpu.speed`
* `hw.memory.size` <=> `hw.gpu.memory.limit`
* `hw.energy` <=> `hw.host.energy` -- are `energy` (and `power`) common metrics or not?

_Use inconsistent linking_

`hw.<metric-type>` metrics require information about parent item, type and location, whereas `hw.<component>.<metric-type>` metrics need less of those.

There's also no specific HW info metric type, that could act as parent when there are no metrics available from the parent component.

_Seem wrong_

* `hw.cpu.speed`: For HW, `frequency` is the unambiguous metric with obvious base unit,
  whereas `speed` is often understood as operations per time, e.g. FLOPS.

_Do not differentiate between base and derived metrics_

Underlying HW and drivers provide base metrics like joules, usage and limits / sizes.

Derived metrics like `.watts` (joules / time) and `.utilization` (usage / limit) do not require HW themselves.
Such metrics could be provided by a generic metric processor for all hardware.

(This is a minor point.)


## The solution

Drop the artificial separation of common, "non-common" and `hw.<component>.<metric-type>` items, and use just:

* `hw.<component>[.subcomponent][.info]`: item providing component attributes, and knowledge that it exists
* `hw.<metric-type>`: item providing metric values + their attributes for a component

And link all of them _in exactly the same way_.

Example HW component metric:

```
hw.gpu.info{hw.id="gpu-1", hw.name="gpu-1-name", pci.bdf="00:0000:0000.1", ...} 1  # HW component
```


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

TODO: Should sub-component info be under hw.component items, like this?

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

Current Intel _XPU Manager_ OTel metrics exporter implements item linking using [alternative 1](#alternative-1) for simplicity.

For now, metric types and their attributes are mix of OTel specified ones and Intel specific ones.

Used metrics names:

* common OTel metrics
* "semi-common" OTel metrics
* `hw.<component>` OTel metrics
* New GPU metrics for OTel `hw.*` namespace:
  * `hw.frequency.*`
  * `hw.gpu.bandwidth.*`
  * `hw.memory.bandwidth.*`
  * `hw.memory.io.*`
  * etc.

Used attributes:

* Common OTel HW attributes
* Other OTel HW attributes
* New Intel GPU attributes:
  * In `com.intel.*` vendor namespace
  * In OTel  `hw.*` namespace: `hw.frequency.*`, `hw.memory.*`
  * In `pci.*` namespace: `pci.{bdf,device_id,lanes,link_gen,vendor_id}`
  * Not namespaced: `aggregation`, `sample.status`, `statistic`

=> some of them conflict with the OTel spec guidance as they are not namespaced,
or use OTel `hw.*` namespace for metrics that are not in the spec yet.

At least those are likely to change in the future based on discussions with OTel upstream and other GPU vendors.


## References

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
