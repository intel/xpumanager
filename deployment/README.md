# Intel XPU Manager
Intel XPU Manager is an in-band node-level tool that provides local/remote GPU management. It is easily integrated into the cluster management solutions and cluster scheduler. GPU users may use it to manage Intel GPUs, locally. 
It supports local command line interface, local library call and remote RESTFul API interface.

***So far, this container image is targeted as a Prometheus exporter.***

The Intel XPU Manager source repository can be found at [intel/xpumanager](https://github.com/intel/xpumanager/).

# Run XPU Manager in Docker

## Enable TLS
Generate certificate for TLS and configure REST user credential:
```sh
xpum_src_root=... # e.g., $HOME/XPUManagerGit
mkdir -p rest/conf
sudo ${xpum_src_root}/install/tools/rest/keytool.sh --owner=root --group=root
sudo ${xpum_src_root}/install/tools/rest/rest_config.py --owner=root --group=root
```
Run the XPUM container:
```sh
xpum_image=...
docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \
--publish 29999:29999 \
--device /dev/dri:/dev/dri \
-v $(pwd)/rest/conf:/opt/xpum/rest/conf:ro \
${xpum_image}
```

## Disable TLS
Configure REST user credential:
```sh
xpum_src_root=... # e.g., $HOME/XPUManagerGit
mkdir -p rest/conf
sudo ${xpum_src_root}/install/tools/rest/rest_config.py --owner=root --group=root
```
Run the XPUM container by passing environment variable XPUM_REST_NO_TLS=1:
```sh
xpum_image=...
docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \
--publish 29999:29999 \
--device /dev/dri:/dev/dri \
-v $(pwd)/rest/conf:/opt/xpum/rest/conf:ro \
-e XPUM_REST_NO_TLS=1 \
${xpum_image}
```

## Support Fabric Throughput
To enable XPUM for reporting fabric throughput metrics, the container must be run in the 'host' network mode (***--network host***).
To avoid port conflict with host network port assignment, you can pass environment variable ***XPUM_REST_PORT*** to the container to specify the XPUM REST server port. For example:


```sh
docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \
--network host \
--device /dev/dri:/dev/dri \
-v $(pwd)/rest/conf:/opt/xpum/rest/conf:ro \
-e XPUM_REST_NO_TLS=1 \
-e XPUM_REST_PORT=12345 \
${xpum_image}
```

## Support PCIe Throughput
PCIe throughput metrics collection depends on the kernel module '**msr**'. It should be loaded on the host by "***modprobe msr***".  

And this metrics collection is not started in XPUM by default. To make XPUM start to collect it, user needs to pass environment variable ***XPUM_METRICS*** which includes the PCIe throughput metrics index.  

This example shows how to get the list of metrics index from XPUM daemon help text:
```sh
docker run --rm --entrypoint /opt/xpum/bin/xpumd ${xpum_image} -h
```
This example shows how to make XPUM in container start to collect PCIe throughput metrics by passing the environment variable ***XPUM_METRICS***:
```sh
docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \
--cap-add=SYS_RAWIO \
--publish 29999:29999 \
--device /dev/dri:/dev/dri \
--device /dev/cpu:/dev/cpu \
-v /sys/firmware/acpi/tables/MCFG:/pcm/sys/firmware/acpi/tables/MCFG:ro \
-v /proc/bus/pci/:/pcm/proc/bus/pci/ \
-v /proc/sys/kernel/nmi_watchdog:/pcm/proc/sys/kernel/nmi_watchdog \
-v $(pwd)/rest/conf:/opt/xpum/rest/conf:ro \
-e XPUM_REST_NO_TLS=1 \
-e XPUM_METRICS=0-37 \
${xpum_image}
```
