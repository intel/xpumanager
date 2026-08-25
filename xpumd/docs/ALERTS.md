# GPU state / metric alerts

Contents:

* [Description](#description)
* [Alert rules](#alert-rules)
  - [Alert resolving on rule changes](#alert-resolving-on-rule-changes)
  - [Daemon internal telemetry alerts](#daemon-internal-telemetry-alerts)
* [Setup](#setup)


## Description

When health management is enabled for the Kubernetes Intel GPU DRA
driver, it automatically sets health state and device taints for the
GPUs, based on the information provided by the XPUM daemon.

It does not inform cluster admin about these detected HW state changes though,
but one could use e.g. [Prometheus Alertmanager](https://prometheus.io/docs/alerting/latest/overview/)
for that.

Alerts are specified in `PrometheusRule` CRD, handled by the
Prometheus operator. Each alert rule has a name, a Prometheus metric
query triggering the alert, and some extra annotations & labels for
their severity and description. Alertmanager then handles sending
notifications for alerts in "firing" state.

> [!NOTE]
> `PrometheusRule` CRDs are understood also e.g. by Grafana Mimir,
> Thanos and VictoriaMetrics services, and natively supported by AWS AMP
> and Google GCP platforms.


## Alert rules

Alert rule file provides examples for potentially relevant GPU
health related metrics currently supported by the XPUM daemon:

* [xpum-alert-rules.yaml](xpum-alert-rules.yaml)

All GPUs do not provide all the metrics, and they can have different
maximum values.  Alerts are also intended for continuing conditions
and their Prometheus metric rules specify a period "FOR" over which
that condition is evaluated.

Thus, prior to deploying the alert rules:

- **Review evaluation periods**: audit how long the conditions should exist before sending notifications
- **Evaluate relevance**: remove rules that are not relevant for given cluster use-cases

> [!NOTE]
> Depending on Prometheus Operator / `kube-prometheus` Helm chart
> settings, rule file `metadata.labels` may need to be updated to match
> Prometheus `ruleSelector`.


### Alert resolving on rule changes

As long as only the rule conditions for an alert are changed (not
other alert details, nor Alertmanager configuration), Alertmanager
will resolve that alert if its new rule conditions do not trigger any
more with metric values.  E.g. after power limit for an alert is
increased.


### Daemon internal telemetry alerts

The last rules in the file (`GpuNotAccessibleToXpumd`,
`XpumdDeviceFilesOutOfSync`, `XpumdRestartLimitExhausted`,
`XpumdRestartStateUnwritable`) are about xpumd
itself (rather than GPU device health). They fire when non-monitored GPU
devices are detected. The related metrics come from the
[`intel_device_watch`](../extension/inteldevicewatch/README.md) extension.

These are `otelcol_*` metrics, i.e. part of the collector's *internal*
telemetry, which is served separately from the Prometheus exporter endpoint the
GPU metrics come from.  By default it is bound to `localhost`, which is not
reachable from outside the container, so it needs to be exposed:

```bash
  --set 'config.service.telemetry.metrics.readers[0].pull.exporter.prometheus.host=0.0.0.0' \
  --set 'config.service.telemetry.metrics.readers[0].pull.exporter.prometheus.port=8888'
```

Also, Prometheus needs to scrape that port. The `ServiceMonitor` created by the
chart covers only the GPU metrics port, so add a scrape target for port 8888 of
the daemon pods, e.g. with an additional `ServiceMonitor` or `PodMonitor`.

These alerts name the node in their annotations, so that target needs to relabel
`__meta_kubernetes_pod_node_name` to `node`, the way the chart's `ServiceMonitor`
does for the GPU metrics port.

## Setup

See [MONITORING](MONITORING.md) for setting up Prometheus operator /
Alertmanager with `kube-prometheus`.

After reading [Alert rules](#alert-rules) section and updating rules
appropriately, apply GPU metric alert rules to the Prometheus /
Alertmanager namespace:

```
kubectl apply -n <namespace> -f xpum-alert-rules.yaml
```

One could start with lowered thresholds, to verify that alerts work
e.g. by viewing the resulting alerts (if any) from Grafana.

Once everything works, one can revert to more suitable thresholds, and
configure Alertmanager to automatically send alert notifications.
