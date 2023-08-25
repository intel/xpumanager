# Intel XPU System Management Interface
Intel XPU System Management Interface is an in-band node-level tool that provides local GPU management. It is easily integrated into the cluster management solutions and cluster scheduler. GPU users may use it to manage Intel GPUs, locally. 
It supports local command line interface and local library call interface. 

## Intel XPU System Management Interface feature
* Provide GPU basic information, including GPU model, frequency, GPU memory capacity, firmware version
* Provide lots of GPU telemetries, including GPU utilization, performance metrics, GPU memory bandwidth, temperature
* Provide GPU health status, memory health, temperature health
* GPU diagnotics through different levels of GPU test suites
* GPU firmware update
* Get/change GPU settings, including power limit, GPU frequency, standby mode and scheduler mode
* Support K8s and can export GPU telemetries to Prometheus

## Suppored Devices
* Intel(R) Data Center Flex Series GPU
* Intel(R) Data Center Max Series GPU

## Supported OS
* Ubuntu 20.04.3/22.04
* RHEL 8.5/8.6
* CentOS 8/9 Stream
* CentOS 7.4/7.9
* SLES 15 SP3/SP4
* Debian 10.13
  


## Intel XPU System Management Interface Command Line Interface
* Show GPU basic information
![Show GPU basic information](doc/img/cli_gpu_info.PNG)
  

* Change GPU settings
![Show GPU settings](doc/img/cli_settings.PNG)
  
  
## Intel XPU System Management Interface Installation
Please follow [XPU System Management Interface Installation Guide](doc/smi_install_guide.md) to install/uninstall Intel XPU System Management Interface. 

### Start to use Intel XPU System Management Interface
By default, Intel XPU System Management Interface is installed the folder, /usr/bin, /usr/lib and /usr/lib64. The command line tool is /usr/bin/xpu-smi. Please refer to [XPU System Management Interface CLI User Guide](doc/smi_user_guide.md) for how to use the command line tool. 
