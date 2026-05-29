<!---

Copyright (C) 2026 Intel Corporation

SPDX-License-Identifier: MIT

-->

# Intel(R) XPU Manager and XPU System Management Interface
Intel(R) XPU Manager is a free and open-source tool for monitoring and managing Intel Data Center GPUs.
It is designed to simplify administration, and to help in improving device utilization and maximizing their reliability and uptime.
Intel(R) XPU System Management Interface (XPU-SMI) is a command line interface (CLI) to manage GPUs locally.

## Table of Contents

- [Intel(R) XPU Manager features](#intelr-xpu-manager-features)
- [SMI output examples](#smi-output-examples)
  - [Default xpu-smi output](#default-xpu-smi-output)
  - [Running xpu-smi discovery](#running-xpu-smi-discovery)
- [How to get XPU Manager](#how-to-get-xpu-manager)
  - [Linux CLI tools (xpu-smi)](#linux-cli-tools-xpu-smi)
  - [Windows CLI tools](#windows-cli-tools)
  - [GPU info exporter (xpumd)](#gpu-info-exporter-xpumd)
- [Supported Devices](#supported-devices)
- [Supported OSes](#supported-oses)
- [Runtime Dependencies](#runtime-dependencies)
- [Release cadence](#release-cadence)
- [Documentation](#documentation)

## Intel(R) XPU Manager features

* Administration:
	* GPU discovery and information - name, model, serial, stepping, location, frequency, memory capacity, firmware version
	* GPU topology
	* GPU Firmware updating, including GPU GFX firmware and AMC (Add-in card Management Controller) firmware updating. 
* Monitoring:
	* GPU telemetry – utilization, power, frequency, temperature, fabric speed, memory throughput, errors
	* GPU health – memory, power, temperature, fabric port
	* [GPU metric exporter daemon](xpumd/README.md)
* Configuration:
	* GPU Settings - GPU power limits, frequency range, standby mode, scheduler mode, ECC On/Off, fabric port status
 
## SMI output examples

Some fields in the `xpu-smi` output example below may vary depending on platform used.

### Default xpu-smi output

```
$ xpu-smi
+----------------------------------------------+----------------------------+------------------------+
|            Intel XPU-SMI v2.0    Driver: 9FF4708BCC22C0EE6FD801A    Level Zero: 1.27.0             |
+----------------------------------------------+----------------------------+------------------------+
| GPU  Name                    Persistence-M   | Bus-Id            Disp.A   |   Volatile Uncorr. ECC |
| Fan  Temp  Pwr:Usage/Cap                     | Memory-Usage               |   GPU-Util  Compute M. |
+----------------------------------------------+----------------------------+------------------------+
|   0  Intel(R) Arc(TM) B580   Off             | 0000:03:00.0      Off      |               Disabled |
| N/A    25C  66W / 380W                       | 239MiB / 12216MiB          |      0%        Default |
+----------------------------------------------+----------------------------+------------------------+

+-------+-----------+--------+----------------+------------------------------------------------------+
| Processes:                                                                                         |
|   GPU         PID    Type    Process Name                                         GPU Memory Usage |
+-------+-----------+--------+----------------+------------------------------------------------------+
|     0     2874292     C      xpu-smi                                                       206 MiB |
+-------+-----------+--------+----------------+------------------------------------------------------+
```
### Running xpu-smi discovery

```
$ xpu-smi discovery -d 0
+-----------+--------------------------------------------------------------------------------------+
| Device ID | Device Information                                                                   |
+-----------+--------------------------------------------------------------------------------------+
| 0         | Device Type: GPU                                                                     |
|           | Device Name: Intel(R) Arc(TM) B580 Graphics                                          |
|           | Device State: normal                                                                 |
|           | PCI Device ID: 0xe20b                                                                |
|           | Vendor Name: Intel(R) Corporation                                                    |
|           | SOC UUID: 00000000-0000-0003-0000-0000e20b8086                                       |
|           | Serial Number: unknown                                                               |
|           | Core Clock Rate: 2850 MHz                                                            |
|           | Stepping: A0                                                                         |
|           | SKU Type: Production ES                                                              |
|           |                                                                                      |
|           | Driver Version: 9FF4708BCC22C0EE6FD801A                                              |
|           | Kernel Version: 6.19.0-rc6                                                           |
|           | GFX Firmware Name: GFX                                                               |
|           | GFX Firmware Version:                                                                |
|           | GFX Firmware Status: normal                                                          |
|           |                                                                                      |
|           | PCI BDF Address: 0000:03:00.0                                                        |
|           | PCI Slot: PCIEx16(G5)                                                                |
|           | PCIe Generation: 4                                                                   |
|           | PCIe Max Link Width: 8                                                               |
|           | PCIe Max Bandwidth: 15.75 GB/s                                                       |
|           |                                                                                      |
|           | Memory Physical Size: 12216.00 MiB                                                   |
|           | Max Mem Alloc Size: 11605.20 MiB                                                     |
|           | ECC State: disabled                                                                  |
|           | Number of Memory Channels: 12                                                        |
|           | Memory Bus Width: 384                                                                |
|           | Max Hardware Contexts: 65536                                                         |
|           | Max Command Queue Priority: 0                                                        |
|           |                                                                                      |
|           | Number of EUs: 160                                                                   |
|           | Number of Tiles: 1                                                                   |
|           | Number of Slices: 5                                                                  |
|           | Number of Sub Slices per Slice: 4                                                    |
|           | Number of Threads per EU: 8                                                          |
|           | Physical EU SIMD Width: 16                                                           |
|           | Number of Media Engines: 2                                                           |
|           | Number of Media Enhancement Engines: 2                                               |
+-----------+--------------------------------------------------------------------------------------+
```

 
## How to get XPU Manager

### Linux CLI tools (xpu-smi)
`xpu-smi` package is available from the [Intel package repositories](https://dgpu-docs.intel.com/). One can also manually download / install the latest `xpu-smi` binary package from the XPUM [release page](https://github.com/intel/xpumanager/releases).

### Windows CLI tools
Latest installers / binaries can be downloaded from the XPUM [release page](https://github.com/intel/xpumanager/releases).

### GPU info exporter (xpumd)
XPUM Daemon is available as a container image, please see [`xpumd` README](xpumd/README.md).

## Supported Devices

* Intel(R) Arc Pro Series GPU ([GPU Driver Installation Guides](https://dgpu-docs.intel.com/installation-guides/index.html))
 
## Supported OSes

* XPU-SMI
	* Ubuntu 24.04.3
	* Windows Server 2022 (limited features including: GPU device info, GPU telemetry, GPU firmware update and GPU configuration)

## Runtime Dependencies

For Ubuntu 22.04 and later, required Intel drivers can be installed from the [Intel package repositories](https://dgpu-docs.intel.com/) 

Dependency releases:

 - [LevelZero Loader](https://github.com/oneapi-src/level-zero/releases) (v1.27.0 or later) and [Intel Graphics Compute Runtime](https://github.com/intel/compute-runtime/releases)
 - [meetee](https://github.com/intel/metee/releases)
 - [IGSC](https://github.com/intel/igsc/releases)
 - [CLI11](https://github.com/CLIUtils/CLI11/releases) (v2.6.2 or later)

 System packages:

For system package dependencies, please refer to [Prerequisites](./BUILDING.md#prerequisites)

## Release cadence

XPU Manager uses [semantic versioning](https://semver.org/) in the form `MAJOR.MINOR.PATCH`.

- Minor versions are released periodically as new features and platform support are stabilized.
- Patch versions are released as needed for critical fixes and security updates.

## Documentation

* Refer to [Building XPU Manager Installer](./BUILDING.md) to build XPU Manager installer packages. 
* For Contributions: Please refer to [CONTRIBUTING](./CONTRIBUTING.md)
* When reporting issues, please use the suggested templates: https://github.com/intel/xpumanager/issues

  
