# Intel XPU Manager
Intel XPU Manager is a free and open-source solution for monitoring and managing Intel data center GPUs.

It is designed to simplify administration, maximize reliability and uptime, and improve utilization.

Intel XPU Manager can be used standalone through its command line interface (CLI) to manage GPUs locally, or through its RESTful APIs to manage GPUs remotely.

3rd party open-source and commercial workload and cluster managers, job schedulers, and monitoring solutions can also integrate the Intel XPU Manager to support Intel data center GPUs.

## Intel XPU Manager features
* Administration:
	* GPU discovery and information - name, model, serial, stepping, location, frequency, memory capacity, firmware version
	* GPU topology and grouping
	* GPU Firmware updating
* Monitoring:
	* GPU health – memory, power, temperature, fabric port, etc.
	* GPU telemetry – utilization, power, frequency, temperature, fabric speed, memory throughput, errors
* Diagnostics:
	* 3 levels of GPU diagnostic tests
* Configuration:
	* GPU Settings - GPU power limits, frequency range, standby mode, scheduler mode, ECC On/Off, performance factor, fabric port status
	* GPU policies - Throttle GPU when the temperature set threshold is reached 
* Supported Frameworks:
	* Prometheus exporter, Docker container support, Icinga plugin

## Supported Devices
* Intel Data Center Flex Series GPU ([GPU Driver Installation Guides](https://dgpu-docs.intel.com/installation-guides/index.html))
 
## Supported OSes
* Ubuntu 20.04.3/22.04
* RHEL 8.5/8.6
* CentOS 8 Stream
* CentOS 7.4/7.9
* SLES 15 SP3
* Windows Server 2022 (limited features including: GPU device info, GPU telemetry and GPU settings)
  
## Intel XPU Manager Architecture
![Intel XPU Manager Architecture](doc/img/architecture.PNG)
  
## GPU telemetry exported from Intel XPU Manager to Grafana
![GPU telemetry exported from Intel XPU Manager to Grafana](doc/img/Grafana.PNG)
  
## Intel XPU Manager Documentation
* Refer to the [XPU Manager Installation Guide](doc/Install_guide.md) for how to install/uninstall Intel XPU Manager.
* Refer to the [XPU Manager CLI User Guide](doc/CLI_user_guide.md) to start using Intel XPU Manager.
* Refer to [DockerHub](https://hub.docker.com/r/intel/xpumanager) for a Docker container image that can be used as a Prometheus exporter in a Kubernetes environment.
* Refer to [Building XPU Manager Installer](BUILDING.md) to build XPU Manager installer packages. 
