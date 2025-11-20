# XPU Exporter

Metrics exporter daemon for Intel GPU devices.

## Building

Clone required repositories and build the exporter.

```bash
git clone https://github.com/marquiz/xpu-exporter xpu-exporter
git clone https://github.com/marquiz/level-zero-go level-zero-go
cd xpu-exporter
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
