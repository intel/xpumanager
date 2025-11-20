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
