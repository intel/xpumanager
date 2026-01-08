# xpu-exporter

![Version: 0.1.0](https://img.shields.io/badge/Version-0.1.0-informational?style=flat-square) ![Type: application](https://img.shields.io/badge/Type-application-informational?style=flat-square) ![AppVersion: main](https://img.shields.io/badge/AppVersion-main-informational?style=flat-square)
A Helm chart for Intel(R) XPU Exporter

## Installation

Helm login to ghcr.io:

```bash
helm registry login ghcr.io -u GITHUB_USERNAME
```

Create an image pull secret in your Kubernetes cluster:

```bash
kubectl create secret docker-registry ghcr-secret \
  --docker-server=ghcr.io/intel \
  --docker-username=GITHUB_USERNAME \
  --docker-password=GITHUB_PERSONAL_ACCESS_TOKEN \
```

Install the chart:

```bash
helm install xpu-exporter oci://ghcr.io/marquiz/xpu-exporter/charts/xpu-exporter \
  --set imagePullSecrets[0].name=ghcr-secret \
  --version 0.0.0-main
```

By default no metrics exporters are enabled. To enable exporters, the corresponding
values must be set. For example, to enable the OTLP gRPC exporter:

```bash
helm install xpu-exporter oci://ghcr.io/marquiz/xpu-exporter/charts/xpu-exporter \
  --set imagePullSecrets[0].name=ghcr-secret \
  --set config.exporters.grpc.enabled=true \
  --set config.exporters.grpc.endpoint="otel-collector:4317" \
```

## Values

### XPU Exporter Configuration

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| config.exporters | object | `{}` | Configuration for exporters (https://opentelemetry.io/docs/collector/configuration/#exporters) |
| config.extensions | object | `{}` | Configuration for extensions (https://opentelemetry.io/docs/collector/configuration/#extensions) |
| config.processors | object | `{}` | Configuration for processors (https://opentelemetry.io/docs/collector/configuration/#processors) |
| config.receivers | object |   | Configuration for receivers (https://opentelemetry.io/docs/collector/configuration/#receivers) |
| config.receivers.sysman | object |   | Configuration for the custom Level-Zero Sysman receiver. |
| config.receivers.sysman.collect_interval | string | `"30s"` | Metrics data collection interval. Must be at least twice the sample_interval. |
| config.receivers.sysman.log_level | string | `"info"` | Log level (debug, info, warn, error) |
| config.receivers.sysman.sample_interval | string | `"1s"` | Sample interval for the high-frequency metrics. |
| config.service | object | `{"pipelines":{"metrics":{"receivers":["sysman"]}}}` | Configuration for service (https://opentelemetry.io/docs/collector/configuration/#service) |

### Other Values

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| affinity | object | `{}` | [Affinity](https://kubernetes.io/docs/concepts/scheduling-eviction/assign-pod-node/#affinity-and-anti-affinity) for the pods |
| fullnameOverride | string | `""` | Override the fully qualified app name |
| image.pullPolicy | string | `"Always"` | Image pull policy |
| image.repository | string | `"ghcr.io/marquiz/xpu-exporter/xpu-exporter"` | Image repository |
| image.tag | string | `""` | Image tag, defaults to Chart.AppVersion |
| imagePullSecrets | list | `[]` | [Image pull secrets](https://kubernetes.io/docs/concepts/containers/images#specifying-imagepullsecrets-on-a-pod) |
| nameOverride | string | `""` | Override the chart name |
| nodeSelector | object | `{}` | Node selector for pod placement |
| podAnnotations | object | `{}` | Annotations to add to the pod |
| podSecurityContext | object | `{}` | [Pod security context](https://kubernetes.io/docs/tasks/configure-pod-container/security-context/#set-the-security-context-for-a-pod) |
| resources | object | `{}` | [Resource requests and limits](https://kubernetes.io/docs/concepts/configuration/manage-resources-containers/) of the container |
| securityContext | object | `{"privileged":true,"runAsUser":0}` | [Container security context](https://kubernetes.io/docs/tasks/configure-pod-container/security-context/#set-the-security-context-for-a-container) |
| service.create | bool | `false` | Create service |
| service.port | int | `8080` | Service port |
| service.type | string | `"ClusterIP"` | Service type |
| serviceAccount.annotations | object | `{}` | Annotations to add to the service account |
| serviceAccount.create | bool | `true` | Create service account |
| serviceAccount.name | string | `""` | The name of the service account to use. If not specified and create is true, a name is generated using the fully qualified app name. |
| tolerations | list | `[]` | [Tolerations](https://kubernetes.io/docs/concepts/scheduling-eviction/taint-and-toleration/) for the pods |
