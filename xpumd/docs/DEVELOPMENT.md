# Development

- [Building](#building)
- [Running](#running)
- [Testing Prometheus exporter](#testing-prometheus-exporter)
- [Testing device info exporter](#testing-device-info-exporter)
- [Building container image](#building-container-image)
- [Testing container image](#testing-container-image)
  - [SELinux and Podman](#selinux-and-podman)
  - [Testing container image with stub driver](#testing-container-image-with-stub-driver)
- [Extracting sources from container image](#extracting-sources-from-container-image)
- [Testing in single-node cluster](#testing-in-single-node-cluster)
  - [Load container image](#load-container-image)
    - [Kind cluster](#kind-cluster)
    - [Containerd-based cluster](#containerd-based-cluster)
  - [Deploy with Helm](#deploy-with-helm)
- [Driver stack updates](#driver-stack-updates)

## Building

See the build requirements in Level-Zero Go Bindings
[README](../level-zero-go/README.md#requirements).

Clone the repository:

```bash
git clone https://github.com/intel/xpumanager
```

Switch to it:

```bash
cd xpumanager/xpumd
```

And build the daemon:

```bash
make build
```

## Running

```bash
sudo ./dist/xpumd --config config-example.yaml
```

> [!NOTE]
> Extra privileges are required to get all the metrics, but some metrics
> are available also without `sudo`, see [Metrics](../README.md#metrics).

> [!NOTE]
> If not running this through normal user session, one may need to specify
> `intelxpuinfo` exporter socket directory with e.g. `XDG_RUNTIME_DIR=$PWD`
> environment variable (or full socket path with the
> `exporters.intelxpuinfo.endpoint` config option).

## Testing Prometheus exporter

In another terminal:

```bash
curl --no-progress-meter http://localhost:8080/metrics
```

> [!NOTE]
> Example config restricts access to localhost for security reasons.
> If this is run on another host, `xpumd` needs to be started with
> `--set exporters.prometheus.endpoint=0.0.0.0:8080` option.

## Testing device info exporter

In another terminal, run the test client to receive device health information and changes:

```bash
sudo ./dist/xpuinfo-cli
```

Its output should look something like this:

```bash
info:
    uuid: 8680457d-0800-0000-0002-000000000000
    model: Intel(R) Graphics
    pci: null
health:
    - name: memory
      status: 0
...
```

## Building container image

```bash
docker build -t registry.local/xpumd:latest .
```

## Testing container image

Test the container with example config from the image:

```bash
docker run -it --rm --user 0 --cap-drop ALL --cap-add SYS_ADMIN \
  --device /dev/dri --publish 8080:8080 registry.local/xpumd:latest \
  --config /etc/xpumd/config-example.yaml
```

One can also modify the config, e.g. to drop the local gRPC health endpoint:

```bash
sed -i s/intelxpuinfo,// config-example.yaml
```

Map the modified config inside container, and ask daemon to use it:

```bash
docker run -it --rm --user 0 --cap-drop ALL --cap-add SYS_ADMIN \
  --volume $PWD/config-example.yaml:/etc/xpumd/config.yaml:ro \
  --device /dev/dri --publish 8080:8080 registry.local/xpumd:latest \
  --config /etc/xpumd/config.yaml
```

Query Prometheus metrics:

```bash
curl --no-progress-meter http://localhost:8080/metrics
```

### SELinux and Podman

If running on a distro with SELinux enabled and `docker` being
provided by `podman` i.e. container being run with normal user
privileges, add `--security-opt label=disable` Docker option,
otherwise contents of host volume mounts are inaccessible.


### Testing container image with stub driver

The image also includes a stub Level-Zero loader library which can be used to
test the daemon without a real GPU or driver.

Use `LD_LIBRARY_PATH` to load the stub driver and `SYSMAN_STUB_CONFIG` to
specify the config file:

```bash
docker run -it --rm --user 0 \
  -e LD_LIBRARY_PATH=/usr/local/lib/xpumd/level-zero-stub \
  -e SYSMAN_STUB_CONFIG=/etc/xpumd/level-zero-stub/example-config.yaml \
  --publish 8080:8080 registry.local/xpumd:latest \
  --config /etc/xpumd/config-example.yaml
```

To supply test-specific stub state, bind-mount a YAML file into the container
and point `SYSMAN_STUB_CONFIG` at that path:

```bash
docker run -it --rm --user 0 \
  -e LD_LIBRARY_PATH=/usr/local/lib/xpumd/level-zero-stub \
  -e SYSMAN_STUB_CONFIG=/etc/xpumd/level-zero-stub/example-config.yaml \
  --volume $PWD/level-zero-go/level-zero-stub:/etc/xpumd/level-zero-stub:ro \
  --publish 8080:8080 registry.local/xpumd:latest \
  --config /etc/xpumd/config-example.yaml
```

> [!NOTE]
> When using the stub loader, no `/dev/dri` device mapping or GPU-specific
> Linux capabilities are required.

## Extracting sources from container image

For (L)GPL compliance, the container image includes source packages in the
`/sources` directory for (L)GPL-licensed packages that were added on top of the
Ubuntu base image.

To extract these sources from the container image (locally built one is used as an
example here):

1. Run image in a temporary container:

```bash
docker create --name xpumd-temp registry.local/xpumd:latest
```

2. Extract sources from it:

```bash
docker cp xpumd-temp:/sources ./sources
```

3. And remove the container:

```bash
docker rm xpumd-temp
```

4. List the source packages:

```bash
ls -lh ./sources
```

## Testing in single-node cluster

After building the container image, load the image onto the cluster.

### Load container image

#### Kind cluster

```bash
kind load docker-image registry.local/xpumd:latest
```

#### Containerd-based cluster

1. Save image as a tarball (on build machine):

```bash
docker save registry.local/xpumd:latest -o xpumd-latest.tar
```

2. Import tarball to container runtime (on cluster node):

```bash
sudo ctr -n k8s.io images import xpumd-latest.tar
```

(`-n k8s.io` option is needed for images to be visible to Kubernetes / `crictl`.)

### Deploy with Helm

The most straightforward way to deploy the daemon for testing and debugging is
to use privileged mode, which allows the daemon to access all the necessary
devices without Kubernetes resource drivers:

```bash
helm install xpumd charts/xpumd --set image.repository=registry.local/xpumd --set image.pullPolicy=Never \
        --set gpuAccess=none \
        --set securityContextOverride.runAsUser=0 --set securityContextOverride.privileged=true
```

See [Helm chart README](../charts/xpumd/README.md) for other deployment scenarios
and detailed configuration options.

## Integration tests

The project includes a suite of Kubernetes integration tests that verify the
deployment and basic functionality using the
[stub Level-Zero driver](../level-zero-go/level-zero-stub).

### Kind cluster

Run with:

```bash
make test-integration-kind
```

This runs everything locally: builds the container image, creates a disposable
Kind cluster, deploys xpumd via the Helm chart (with stub driver enabled), and
runs the tests.

### Existing cluster

To run the integration tests against an existing cluster:

```bash
make test-integration-existing-cluster IMAGE_REPOSITORY=ghcr.io/intel/xpumanager/xpumd IMAGE_TAG=latest
```

> [!IMPORTANT]
> Ensure that the container image is present in/reachable by the cluster.

## Driver stack updates

XPUMD [helper scripts](../scripts/README.md) and Docker files validate
directly downloaded (driver) DEB and ZIP files against hard-coded
checksums.  If DEB / ZIP file versions are changed, build will error
out after outputting the new checksums (so they can be updated after
validation).
