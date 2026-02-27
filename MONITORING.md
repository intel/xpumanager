# Monitoring XPUM daemon metrics

## Table of Contents

- [Prometheus + Grafana install](#prometheus--grafana-install)
- [XPUMD Helm options](#xpumd-helm-options)
- [View metrics](#view-metrics)
- [Manual metrics verification](#manual-metrics-verification)
- [Dashboard install workarounds](#dashboard-install-workarounds)


## Prometheus + Grafana install

If cluster does not run [Prometheus operator](https://github.com/prometheus-operator/kube-prometheus)
yet, it SHOULD be be installed before enabling monitoring, e.g. by using a Helm chart for it:
https://github.com/prometheus-community/helm-charts/tree/main/charts/kube-prometheus-stack#readme

First verify that following default Grafana options are enabled, for dashboard auto-loading:
```console
$ helm -n monitoring show values prometheus-community/kube-prometheus-stack | grep -B1 -A3 dashboards:
  sidecar:
     dashboards:
       enabled: true
       label: grafana_dashboard
       labelValue: "1"
```

Then install it:

```console
$ helm repo add prometheus-community https://prometheus-community.github.io/helm-charts
$ helm repo update
$ prom_ns=monitoring  # namespace for Prometheus
$ kubectl create ns $prom_ns
$ helm install prometheus-stack prometheus-community/kube-prometheus-stack -n $prom_ns
```


## XPUMD Helm options

Helm chart `prometheus.monitor=true` option enables Prometheus monitoring for the daemon,
and `grafana.dashboards=true` option installs Grafana dashboard for viewing the produced metrics.

NOTE: XPUMD Helm chart assumes Prometheus & Grafana are installed to `monitoring` namespace, using
`prometheus-stack` as the release name for the `kube-prometheus` installation, as in the above example.

If this is not the case, update `grafana.namespace` and `prometheus.release`
chart values to correct values.

(Otherwise XPUMD `serviceMonitor` object and dashboard `configMap` will not be found by
the Prometheus and Grafana services installed by the `kube-prometheus` Helm chart.)

TODO: add screenshot of the dashboard.


## View metrics

Get Grafana service name matching cluster Prometheus installation:

```console
$ prom_ns=monitoring  # set to Prometheus/Grafana namespace
$ grafana_svc=$(kubectl --namespace $prom_ns get svc -l app.kubernetes.io/name=grafana -oname)
```

Port-forward Grafana service to port 3000 on local machine:

```console
$ kubectl port-forward -n $prom_ns $grafana_svc 3000:80
```

Open your browser and navigate to http://localhost:3000.
Login to Grafana with `admin` user and generated password shown with:

```console
$ kubectl get secret -n $(kubectl get svc -A | awk '/grafana/{print $1,$2}') \
  -o jsonpath="{.data.admin-password}" | base64 --decode ; echo
```

(With older Grafana versions, password is `prom-operator`, not one generated to a secret on Grafana install.)


## Manual metrics verification

If Grafana dashboard does not show any metrics, one can check whether Prometheus actually gets them.

Use service name matching your Prometheus installation:

```console
$ prom_ns=monitoring  # Prometheus/Grafana namespace
$ prom_svc=$(kubectl -n $prom_ns get svc | awk '/^[a-z-]*-prometheus /{print $1}')
```

Verify Prometheus found metric endpoint for the XPUMD service, i.e. last number on `curl` output is non-zero:

```console
$ chart=intel-xpumd # XPUMD chart release name
$ prom_url=http://$(kubectl -n $prom_ns get -o jsonpath="{.spec.clusterIP}:{.spec.ports[0].port}" svc/$prom_svc)
$ curl --no-progress-meter $prom_url/metrics | grep scrape_pool_targets.*$chart
```

Then check that OTel HW metrics are available from Prometheus:

```console
$ curl --no-progress-meter $prom_url/api/v1/query? \
  --data-urlencode 'query=hw_gpu_info{service="'$chart'"}' | jq
```


## Dashboard install workarounds

`kube-prometheus` Helm chart should have enabled dashboard auto-loading sidecar:

```console
$ helm show values prometheus-community/kube-prometheus-stack | grep -B2 -A6 dashboards:
```

As explained in the [Grafana Helm chart documentation](https://github.com/grafana-community/helm-charts/tree/main/charts/grafana#sidecar-for-dashboards).

But if that does not work, restarting Grafana (by deleting Grafana pod from `$prom_ns` namespace) should load all the available dashboard configMaps.

If even that fails, one can also load the [dashboard JSON version](charts/xpumd/json/) directly from Grafana.
