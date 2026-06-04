# Monitoring XPUM daemon metrics

## Table of Contents

- [Prometheus + Grafana install](#prometheus--grafana-install)
- [XPUMD Helm options](#xpumd-helm-options)
- [View metrics](#view-metrics)
- [GPU metric alerts](#gpu-metric-alerts)
- [Manual metrics verification](#manual-metrics-verification)
- [Dashboard install workarounds](#dashboard-install-workarounds)


## Pre-conditions

Specify which namespace is used for Prometheus / Grafana (for commands used later on):

```bash
prom_ns=monitoring
```

Create it, if it does not exist yet:

```bash
kubectl create ns $prom_ns
```


## Prometheus + Grafana install

If cluster does not run [Prometheus operator](https://github.com/prometheus-operator/kube-prometheus)
yet, it SHOULD be be installed before enabling monitoring, e.g. by using a Helm chart for it:
https://github.com/prometheus-community/helm-charts/tree/main/charts/kube-prometheus-stack#readme

1. Tell Helm to add Prometheus community as source for charts:

```bash
helm repo add prometheus-community https://prometheus-community.github.io/helm-charts
```

2. Fetch latest repository content:

```bash
helm repo update
```

3. Verify that Grafana options for dashboard auto-loading:

```bash
helm -n $prom_ns show values prometheus-community/kube-prometheus-stack | grep -B1 -A3 dashboards:
```

Are enabled (as they should by default):

```bash
  sidecar:
     dashboards:
       enabled: true
       label: grafana_dashboard
       labelValue: "1"
```

4. Install Prometheus (+ Grafana) to specified namespace:

```bash
helm install prometheus-stack prometheus-community/kube-prometheus-stack -n $prom_ns
```


## XPUMD Helm options

Helm chart `--set prometheus.monitor=true` option enables Prometheus monitoring for the daemon,
and `--set grafana.dashboards=true` option installs Grafana dashboard for viewing the produced metrics.

NOTE: XPUMD Helm chart assumes Prometheus & Grafana are installed to `monitoring` namespace, using
`prometheus-stack` as the release name for the `kube-prometheus` installation, as in the above examples.

If this is not the case, update `grafana.namespace` and `prometheus.release` chart values to correct values.

(Otherwise XPUMD `serviceMonitor` object and dashboard `configMap` will not be found by
the Prometheus and Grafana services installed by the `kube-prometheus` Helm chart.)


## View metrics

Get Grafana service name matching cluster Prometheus installation:

```bash
grafana_svc=$(kubectl --namespace $prom_ns get svc -l app.kubernetes.io/name=grafana -oname)
```

Port-forward Grafana service to port 3000 on local machine:

```bash
kubectl port-forward -n $prom_ns $grafana_svc 3000:80
```

Open your browser and navigate to [http://localhost:3000](http://localhost:3000).
Login to Grafana with `admin` user and password shown with:

```bash
kubectl get secret -n $(kubectl get svc -A | awk '/grafana/{print $1,$2}') \
  -o jsonpath="{.data.admin-password}" | base64 --decode ; echo
```

(With older Grafana versions, password is `prom-operator`, instead
of being generated on Grafana install and stored into a K8s Secret.)

![GPU metrics dashboard](xpumd-dashboard.png)


## GPU metric alerts

K8s DRA driver (listening on XPUMD local gRPC socket) can taint
unhealthy cluster devices, but those do not produce alerts for the
admin. [ALERTS.md](ALERTS.md) provides some examples on setting up
GPU metric based alerts.


## Manual metrics verification

If Grafana dashboard does not show any metrics, one can check whether Prometheus actually gets them.

Specify release name used for the XPUMD chart:

```bash
chart=intel-xpumd
```

Get service name matching the Prometheus installation:

```bash
prom_svc=$(kubectl -n $prom_ns get svc | awk '/^[a-z-]*-prometheus /{print $1}')
```

Get Prometheus service URL:

```bash
prom_url=http://$(kubectl -n $prom_ns get -o jsonpath="{.spec.clusterIP}:{.spec.ports[0].port}" svc/$prom_svc)
```

Verify Prometheus found metric endpoint for the XPUMD service:

```bash
curl --no-progress-meter $prom_url/metrics | grep scrape_pool_targets.*$chart
```

=> Is the last number on `curl` output non-zero?

If yes, check that OTel HW metrics are available from Prometheus:

```bash
curl --no-progress-meter $prom_url/api/v1/query? \
  --data-urlencode 'query=hw_gpu_info{service="'$chart'"}' | jq
```


## Dashboard install workarounds

`kube-prometheus` Helm chart should have enabled dashboard auto-loading sidecar:

```bash
helm show values prometheus-community/kube-prometheus-stack | grep -B2 -A6 dashboards:
```

As explained in the [Grafana Helm chart documentation](https://github.com/grafana-community/helm-charts/tree/main/charts/grafana#sidecar-for-dashboards).

But if auto-loading does not work, restarting Grafana (by deleting Grafana pod from `$prom_ns` namespace) should load all the available dashboard configMaps.

If even that fails, one can also load the [dashboard JSON version](../charts/xpumd/json/) directly from Grafana.
