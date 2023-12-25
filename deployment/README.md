# Intel XPU Manager
Intel XPU Manager is an in-band node-level tool that provides local/remote GPU management. It is easily integrated into the cluster management solutions and cluster scheduler. GPU users may use it to manage Intel GPUs, locally. 
It supports local command line interface, local library call and remote RESTFul API interface.

The Intel XPU Manager source repository can be found at [intel/xpumanager](https://github.com/intel/xpumanager/).

# Run XPU Manager in Docker

## Prerequisites
To run XPUM image
* Install Docker (recommended Docker Engine v23.0 or higher)

To configure RESTful user credential
* Install Python3

## Overview
These examples show how to run XPUM container:

Example #1: Most of XPUM features are enabled. Subcommands `vgpu`, `log` and `topdown` are not supported in the container.
```sh
#${xpum_src_root} is the directory where xpumanager code is stored
git clone --depth 1 https://github.com/intel/xpumanager.git
xpum_src_root=$(pwd)/xpumanager
mkdir -p rest/conf
sudo ${xpum_src_root}/install/tools/rest/keytool.sh --owner=root --group=root
sudo ${xpum_src_root}/install/tools/rest/rest_config.py --owner=root --group=root

#${xpum_image} is XPUM docker image name and tag, you can replace "latest" with the specific version
xpum_image=intel/xpumanager:latest
docker pull ${xpum_image}

#${firmware_image_dir} is the directory where firmware images are stored
firmware_image_dir=$HOME/firmware
mkdir -p ${firmware_image_dir}

docker run --rm --privileged \
--network host \
--device /dev/dri:/dev/dri \
--device /dev/mem:/dev/mem \
$(ls /dev/|grep mei|sed 's/^/--device \/dev\//g'|tr "\n" " ") \
$(ls /dev/|grep ipmi|sed 's/^/--device \/dev\//g'|tr "\n" " ") \
-v /sys/kernel/debug:/sys/kernel/debug \
-v $(pwd)/rest/conf:/usr/lib/xpum/rest/conf:ro \
-v ${firmware_image_dir}:/tmp/firmware:ro \
-e XPUM_REST_PORT=12345 \
${xpum_image}
```

Example #2: Limited features are enabled. It is targeted as a Prometheus exporter.
```sh
#${xpum_src_root} is the directory where xpumanager code is stored
git clone --depth 1 https://github.com/intel/xpumanager.git
xpum_src_root=$(pwd)/xpumanager
mkdir -p rest/conf
sudo ${xpum_src_root}/install/tools/rest/keytool.sh --owner=root --group=root
sudo ${xpum_src_root}/install/tools/rest/rest_config.py --owner=root --group=root

#${xpum_image} is XPUM docker image name and tag, you can replace "latest" with the specific version
xpum_image=intel/xpumanager:latest
docker pull ${xpum_image}

docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \
--publish 29999:29999 \
--device /dev/dri:/dev/dri \
-v $(pwd)/rest/conf:/usr/lib/xpum/rest/conf:ro \
${xpum_image}
```

Please see details below for customized configurations.

## Enable TLS
Generate certificate for TLS and configure RESTful user credential:
```sh
xpum_src_root=$HOME/xpumanager
mkdir -p rest/conf
sudo ${xpum_src_root}/install/tools/rest/keytool.sh --owner=root --group=root
sudo ${xpum_src_root}/install/tools/rest/rest_config.py --owner=root --group=root
```
Run the XPUM container:
>docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \\ \
>--publish 29999:29999 \\ \
>--device /dev/dri:/dev/dri \\ \
>-v \$(pwd)/rest/conf:/usr/lib/xpum/rest/conf:ro \\ \
>\${xpum_image}

## Disable TLS
Configure RESTful user credential:
```sh
xpum_src_root=$HOME/xpumanager
mkdir -p rest/conf
sudo ${xpum_src_root}/install/tools/rest/rest_config.py --owner=root --group=root
```
Run the XPUM container by passing environment variable XPUM_REST_NO_TLS=1:
>docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \\ \
>--publish 29999:29999 \\ \
>--device /dev/dri:/dev/dri \\ \
>-v \$(pwd)/rest/conf:/usr/lib/xpum/rest/conf:ro \\ \
>-e XPUM_REST_NO_TLS=1 \\ \
>\${xpum_image}

## Support Xe Link Throughput
To enable XPUM for reporting Xe Link throughput metrics, the container must be run in the 'host' network mode (***--network host***).
To avoid port conflict with host network port assignment, you can pass environment variable ***XPUM_REST_PORT*** to the container to specify the XPUM RESTful server port. For example:

>docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \\ \
>--network host \\ \
>--device /dev/dri:/dev/dri \\ \
>-v \$(pwd)/rest/conf:/usr/lib/xpum/rest/conf:ro \\ \
>-e XPUM_REST_NO_TLS=1 \\ \
>-e XPUM_REST_PORT=12345 \\ \
>\${xpum_image}

## Support PCIe Throughput
PCIe throughput metrics collection depends on the kernel module '**msr**'. It should be loaded on the host by "***modprobe msr***".  

And this metrics collection is not started in XPUM by default. To make XPUM start to collect it, user needs to pass environment variable ***XPUM_METRICS*** which includes the PCIe throughput metrics index.  

This example shows how to get the list of metrics index from XPUM daemon help text:
```sh
docker run --rm --entrypoint /usr/bin/xpumd ${xpum_image} -h
```
This example shows how to make XPUM in container start to collect PCIe throughput metrics by passing the environment variable ***XPUM_METRICS***:
>docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \\ \
>--cap-add=SYS_RAWIO \\ \
>--publish 29999:29999 \\ \
>--device /dev/dri:/dev/dri \\ \
>--device /dev/cpu:/dev/cpu \\ \
>--device /dev/mem:/dev/mem \\ \
>-v /sys/firmware/acpi/tables/MCFG:/pcm/sys/firmware/acpi/tables/MCFG:ro \\ \
>-v /proc/bus/pci/:/pcm/proc/bus/pci/ \\ \
>-v /proc/sys/kernel/nmi_watchdog:/pcm/proc/sys/kernel/nmi_watchdog \\ \
>-v \$(pwd)/rest/conf:/usr/lib/xpum/rest/conf:ro \\ \
>-e XPUM_REST_NO_TLS=1 \\ \
>-e XPUM_METRICS=0-38 \\ \
>\${xpum_image}

## Support GFX Firmware Management
To support GFX firmware update in the container, please add the host Intel Management Engine Interface device '***/dev/mei?***' to the container. For example:
>docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \\ \
>--device /dev/dri:/dev/dri \\ \
>\$(ls /dev/|grep mei|sed 's/^/--device \/dev\//g'|tr "\n" " ") \\ \
>-v \${firmware_image_dir}:/tmp/firmware:ro \\ \
>-v \$(pwd)/rest/conf:/usr/lib/xpum/rest/conf:ro \\ \
>-e XPUM_REST_NO_TLS=1 \\ \
>\${xpum_image}

## Support AMC Firmware Management
XPUM supports IPMI protocol and Redfish host interface to update AMC (Add-in-card Management Controller) firmware. Please refer to [releases/notice](https://github.com/intel/xpumanager/releases) for more details.

OPTION #1 : IPMI protocol. Please add the host device '***/dev/ipmi0***' to the container. For example:

>docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \\ \
>--device /dev/dri:/dev/dri \\ \
>\$(ls /dev/|grep ipmi|sed 's/^/--device \/dev\//g'|tr "\n" " ") \\ \
>-v \${firmware_image_dir}:/tmp/firmware:ro \\ \
>-v \$(pwd)/rest/conf:/usr/lib/xpum/rest/conf:ro \\ \
>-e XPUM_REST_NO_TLS=1 \\ \
>\${xpum_image}

OPTION #2 : Redfish host interface. Please add the host device '***/dev/mem***' to the container and be run in the 'host' network mode (***--network host***). Extended privileges (***--privileged***) also need to be added to the container. For example:

>docker run --rm --privileged \\ \
>--network host \\ \
>--device /dev/dri:/dev/dri \\ \
>--device /dev/mem:/dev/mem \\ \
>-v \${firmware_image_dir}:/tmp/firmware:ro \\ \
>-v \$(pwd)/rest/conf:/usr/lib/xpum/rest/conf:ro \\ \
>-e XPUM_REST_NO_TLS=1 \\ \
>\${xpum_image}

## Support Device Configuration
To support device configuration, Please give extended privileges (***--privileged***) to the container. For example:

>docker run --rm --privileged \\ \
>--device /dev/dri:/dev/dri \\ \
>-v \$(pwd)/rest/conf:/usr/lib/xpum/rest/conf:ro \\ \
>-e XPUM_REST_NO_TLS=1 \\ \
>\${xpum_image}

## Disable RESTFul API
If you do not use the container as a Prometheus exporter, you can disable RESTFul API by passing environment variable ***XPUM_REST_DISABLE***  and then no credential configuration is required. For example:
>docker run --rm --cap-drop ALL --cap-add=SYS_ADMIN \\ \
>--device /dev/dri:/dev/dri \\ \
>-e XPUM_REST_DISABLE=1 \\ \
>\${xpum_image}
