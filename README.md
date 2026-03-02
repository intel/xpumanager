# Intel(R) XPU Manager and XPU System Management Interface
Intel(R) XPU Manager is a free and open-source tool for monitoring and managing Intel data center & Edge GPUs.
It is designed to simplify administration, maximize reliability and uptime, and improve utilization.
Intel(R) XPU System Management Interface (XPU-SMI) is a command line interface (CLI) to manage GPUs locally.

## Intel(R) XPU Manager features
* Administration:
	* GPU discovery and information - name, model, serial, stepping, location, frequency, memory capacity, firmware version
	* GPU topology and grouping
	* GPU Firmware updating, including GPU GFX firmware and AMC (Add-in card Management Controller) firmware updating. 
* Monitoring:
	* GPU telemetry – utilization, power, frequency, temperature, fabric speed, memory throughput, errors
	* GPU health – memory, power, temperature, fabric port, etc.
	* [GPU metric exporter daemon](xpumd/README.md)
* Diagnostics:
	* 3 levels of GPU diagnostic tests
	* Pre-check GPU hardware and driver critical issues
	* GPU log collection for the issue investigation
* Configuration:
	* GPU Settings - GPU power limits, frequency range, standby mode, scheduler mode, ECC On/Off, performance factor, fabric port status
	* GPU policies - Throttle GPU when the temperature set threshold is reached 
 
## CLI output of GPU device info, telemetries and firmware update
```
xpumcli discovery -d 0
+-----------+--------------------------------------------------------------------------------------+
| Device ID | Device Information                                                                   |
+-----------+--------------------------------------------------------------------------------------+
| 0         | Device Type: GPU                                                                     |
|           | Device Name: Intel(R) Graphics [0x56c0]                                              |
|           | Vendor Name: Intel(R) Corporation                                                    |
|           | UUID: 01000000-0000-0000-0000-0000004d0000                                           |
|           | Serial Number: LQAC20305316                                                          |
|           | Core Clock Rate: 2050 MHz                                                            |
|           | Stepping: C0                                                                         |
|           |                                                                                      |
|           | Driver Version:                                                                      |
|           | Kernel Version: 5.15.47+prerelease3762                                               |
|           | GFX Firmware Name: GFX                                                               |
|           | GFX Firmware Version: DG02_1.3170                                                    |
|           | GFX Data Firmware Name: GFX_DATA                                                     |
|           | GFX Data Firmware Version: 0x12d                                                     |
|           |                                                                                      |
|           | PCI BDF Address: 0000:4d:00.0                                                        |
|           | PCI Slot: J37 - Riser 1, Slot 1                                                      |
|           | PCIe Generation: 4                                                                   |
|           | PCIe Max Link Width: 16                                                              |
+-----------+--------------------------------------------------------------------------------------+

xpumcli dump -d 0 -m 0,1,2,3
Timestamp, DeviceId, GPU Utilization (%), GPU Power (W), GPU Frequency (MHz), GPU Core Temperature (Celsius Degree)
21:23:00.000,    0, 99.55, 119.61, 1800, 49.00
21:23:01.000,    0, 99.45, 119.36, 1800, 50.00
21:23:02.000,    0, 99.48, 119.55, 1750, 50.50
21:23:03.000,    0, 99.65, 119.24, 1700, 51.00


sudo xpumcli updatefw -d 0 -t GFX -f ATS_M150_512_C0_PVT_ES_032_gfx_fwupdate_SOC1.bin
Device 0 FW version: DG02_1.3170
Image FW version: DG02_1.3172
Do you want to continue? (y/n) y
Start to update firmware
Firmware Name: GFX
Image path: /home/dcm/ATS_M150_512_C0_PVT_ES_032_gfx_fwupdate_SOC1.bin
[============================================================] 100 %
Update firmware successfully.
```

 
## Feature set of XPU Manager, XPU-SMI and XPU-SMI Windows CLI tool
```sh
|                           | XPU-SMI              | XPU-SMI Windows CLI          | amcmcli         |
| :------------------------ | :------------------: | :--------------------------: | :-------------: |
| Device Info and Topology  | Yes                  | Yes                          | No              |
| GPU Telemetries           | Yes (real-time data) | Yes (real-time data)         | No              |
| GPU Firmware Update       | GFX, GFX_Data, AMC   | GFX, GFX_Data, AMC           | AMC (IPMI)      |
| GPU Configuration         | Yes                  | Yes                          | No              |
| GPU Diagnostics           | Yes                  | No                           | No              |
| GPU Health                | Yes                  | No                           | No              |
| GPU Grouping              | No                   | No                           | No              |
| GPU policy                | No                   | No                           | No              |
| Architecture              | Daemon-less          | Daemon-less                  | Daemon-less     |
| Interfaces                | CLI, Library         | CLI, Library                 | CLI             |
```

## How to get XPU Manager, XPU-SMI, Windows CLI and amcmcli binaries. 
You may get the latest installers or binaries in [Releases](https://github.com/intel/xpumanager/releases).

## Supported Devices
* Intel(R) Data Center Flex Series GPU ([GPU Driver Installation Guides](https://dgpu-docs.intel.com/installation-guides/index.html))
* Intel(R) Data Center Max Series GPU ([GPU Driver Installation Guides](https://dgpu-docs.intel.com/installation-guides/index.html))
* Intel(R) Arc Series GPU ([GPU Driver Installation Guides](https://dgpu-docs.intel.com/installation-guides/index.html))
 
## Supported OSes
* XPU-SMI
	* Ubuntu 24.04.3
	* Windows Server 2022 (limited features including: GPU device info, GPU telemetry, GPU firmware update and GPU configuration)

## Documentation
* Refer to [Building XPU Manager Installer](BUILDING.md) to build XPU Manager installer packages. 

  
