# XPUM Daemon

XPUM daemon is a custom
[OpenTelemetry Collector](https://opentelemetry.io/docs/collector/) that
provides:

* Intel GPU metrics
* GPU topology and health information for Kubernetes Intel GPU resource drivers

## Deployment

See the [Helm chart](charts/xpumd/README.md) for deployment instructions.

## Metrics

See the [`intelxpu` receiver documentation](receiver/sysman/documentation.md)
for the list of available GPU metrics and attributes.

## Device info exporter

The XPUM daemon implements a custom exporter that exposes GPU health
information. It serves a custom gRPC API at local Unix socket
(`/run/xpumd/intelxpuinfo.sock` by default).

The device info exporter is enabled by the default configuration file
([`config-example.yaml`](config-example.yaml)) and the Helm chart.

## Development

### Building

Clone required repositories and build the daemon.

```bash
git clone https://github.com/intel/xpumd xpumd
cd xpumd
git clone https://github.com/intel/level-zero-go level-zero-go
make build
```

Run:

```bash
./dist/xpumd --config config-example.yaml
```

In another terminal, test the metrics endpoint:

```bash
curl http://localhost:8080/metrics
```

### Testing device info exporter

Build the project as described above and start the daemon:

```bash
sudo ./dist/xpumd --config config-example.yaml
```

In another terminal, run the test client to receive device health information:
```
$ sudo ./dist/xpuinfo-cli
info:
    uuid: 8680457d-0800-0000-0002-000000000000
    model: Intel(R) Graphics
    pci: null
health:
    - name: memory
      status: 0
...
```

### Building container image

**NOTE:** Level-Zero Go bindings repository must be cloned inside the `xpumd` source tree.

```bash
docker build -t registry.local/xpumd:main .
```

Test the container image:

```bash
docker run --rm --publish 8080:8080 --device /dev/dri --user 0:0 --cap-drop ALL registry.local/xpumd:main --config /etc/xpumd/config-example.yaml
```

```bash
curl http://localhost:8080/metrics
```

### Testing in single-node cluster

After building the container image, load the image onto the cluster. E.g.
with containerd:

```bash
docker save registry.local/xpumd:main  -o xpum-main.tar
sudo ctr -n k8s.io images  import xpum-main.tar
rm xpum-main.tar
```

Deploy the image with the Helm chart:

```bash
helm install xpumd charts/xpumd --set image.repository=registry.local/xpumd --set image.pullPolicy=Never
```
