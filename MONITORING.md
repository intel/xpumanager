# Monitoring XPUM daemon metrics

## Table of Contents

- [Prometheus + Grafana install](#prometheus--grafana-install)
- [XPUMD Helm options](#xpumd-helm-options)
- [View metrics](#view-metrics)
- [Manual verification](#manual-verification)


## Prometheus + Grafana install

If cluster does not run [Prometheus operator](https://github.com/prometheus-operator/kube-prometheus)
yet, it SHOULD be be installed before enabling monitoring, e.g. by using a Helm chart for it:
https://github.com/prometheus-community/helm-charts/tree/main/charts/kube-prometheus-stack#readme

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

If this is not the case, update `kubePrometheus.namespace` and `kubePrometheus.release`
chart values to correct values.

(Otherwise XPUMD `serviceMonitor` object and dashboard `configMap` will not be found by
the Prometheus and Grafana services installed by the `kube-prometheus` Helm chart.)

TODO: add screenshot of the dashboard.


## View metrics

Use port-forward to access Grafana

```
kubectl port-forward service/grafana 3000:80
```

Open your browser and navigate to http://localhost:3000.
Login to Grafana with "admin" user and generated password shown with:

```
kubectl get secret -n $(kubectl get svc -A | awk '/grafana/{print $1,$2}') -o jsonpath="{.data.admin-password}" | base64 --decode ; echo
```

(With older Grafana versions, password is "prom-operator", not one generated to a secret on Grafana install.)


## Manual verification

Check installed Prometheus service names:

```console
$ prom_ns=monitoring  # Prometheus namespace
$ kubectl -n $prom_ns get svc
```

(Object names depend on whether Prometheus was installed from manifests, or Helm,
and the release name given for its Helm install.)

Use service name matching your Prometheus installation:

```console
$ prom_svc=prometheus-stack-kube-prom-prometheus  # Metrics service
```

Verify Prometheus found metric endpoint for the XPUMD service, i.e. last number on `curl` output is non-zero:

```console
$ chart=xpumd # XPUMD chart release name
$ prom_url=http://$(kubectl -n $prom_ns get -o jsonpath="{.spec.clusterIP}:{.spec.ports[0].port}" svc/$prom_svc)
$ curl --no-progress-meter $prom_url/metrics | grep scrape_pool_targets.*$chart
```

Then check that OTel HW metrics are available from Prometheus:

```console
$ curl --no-progress-meter $prom_url/api/v1/query? \
  --data-urlencode 'query=hw_gpu_info{service="'$chart'"}' | jq
```
