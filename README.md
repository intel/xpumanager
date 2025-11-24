# XPU Exporter

Metrics exporter daemon for Intel GPU devices.

## Building

Clone required repositories and build the exporter.

```bash
git clone https://github.com/marquiz/xpu-exporter xpu-exporter
cd xpu-exporter
git clone https://github.com/marquiz/level-zero-go level-zero-go
make build
```

Run:

```bash
./bin/xpu-exporter
...
```

In another terminal, test the metrics endpoint:

```bash
curl http://localhost:8080/metrics
```

## Building container image

**NOTE:** The level-zero-go repository must be cloned in the same directory as the xpu-exporter repository.

```bash
docker build -t registry.local/xpu-exporter:main .
```

Test the container image:

```bash
docker run --rm -p 8080:8080 --device /dev/dri registry.local/xpu-exporter:main
```

## Helm chart

A Helm chart for deploying the XPU Exporter is available in the `charts/xpu-exporter` directory.

### Testing in single-node cluster

First, build the container image as instructed above.

Next, load the image into the kind cluster. E.g. with containerd:

```bash
docker save registry.local/xpu-exporter:main  -o xpum-main.tar
sudo ctr -n k8s.io images  import xpum-main.tar
rm xpum-main.tar
```

Then, install the Helm chart:

```bash
helm install xpu-exporter charts/xpu-exporter --set image.repository=registry.local/xpu-exporter --set image.pullPolicy=Never
```
