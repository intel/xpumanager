# Intel XPU Manager
Intel XPU Manager is an in-band node-level tool that provides local/remote GPU management. It is easily integrated into the cluster management solutions and cluster scheduler. GPU users may use it to manage Intel GPUs, locally. 
It supports local command line interface, local library call and remote RESFTul API interface. 

## Intel XPU Manager feature
* Provide GPU basic information, including GPU model, frequency, GPU memory capacity, firmware version
* Provide lots of GPU telemetries, including GPU utilization, performance metrics, GPU memory bandwidth, temperature
* Provide GPU health status, memory health, temperature health
* GPU diagnotics through different levels of GPU test suites
* GPU firmware update
* Get/change GPU settings, including power limit, GPU frequency, standby mode and scheduler mode
* Support K8s and can export GPU telemetries to Prometheus
  
  

## Intel XPU Manager Architecture
<p align="center">
<img src="https://github.com/intel-sandbox/intel-xpu-manager/blob/main/doc/img/architecture.PNG">
</p>
  
  

## Intel XPU Manager Command Line Interface
* Show GPU basic information
<p align="left">
<img src="https://github.com/intel-sandbox/intel-xpu-manager/blob/main/doc/img/cli_gpu_info.PNG">
</p>  
  

* Show GPU settings
<p align="left">
<img src="https://github.com/intel-sandbox/intel-xpu-manager/blob/main/doc/img/cli_settings.PNG">
</p>
  
  
## GPU telemetries in Grafana exported by Intel XPU Manager
<p align="left">
<img src="https://github.com/intel-sandbox/intel-xpu-manager/blob/main/doc/img/Grafana.PNG">
</p>

