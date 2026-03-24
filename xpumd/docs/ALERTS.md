# GPU state / metric alerts

Contents:

* [Description](#description)
* [Alert rules](#alert-rules)
  - [Alert resolving on rule changes](#alert-resolving-on-rule-changes)
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
