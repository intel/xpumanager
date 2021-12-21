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
  
  
## Intel XPU Manager Installation

### DEB install
```
sudo dpkg -i xpumanager.1.0.0.xxxxxxxx.xxxxxx.xxxxxxxx.deb
```

### DEB uninstall
```
sudo dpkg -r xpumanager
```

### RPM install
```
sudo rpm -i xpumanager.1.0.0.xxxxxxxx.xxxxxx.xxxxxxxx.rpm
```

### RPM relocation install
```
rpm -i --prefix=/opt/abc xpumanager.1.0.0.xxxxxxxx.xxxxxx.xxxxxxxx.rpm
```

### RPM uninstall
```
sudo rpm -e xpumanager
```

### Change daemon monitor metrics
1. edit file "/lib/systemd/system/xpum.service"
   add "-m metric-indexes" to ExecStart. 
   Use "/opt/xpum/bin/xpumd -h" to get detailed info.
   Sample:
   ExecStart=/opt/xpum/bin/xpumd -p /var/xpum_daemon.pid -d /opt/xpum/dump -m 5-8,12
2. Run command "sudo systemctl daemon-reload"
3. Run command "sudo systemctl restart xpum"
