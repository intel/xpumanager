
# Intel(R) XPU Manager Installation Guide

## Requirements
* Intel GPU driver ([GPU Driver Installation Guides](https://dgpu-docs.intel.com/installation-guides/index.html)). XPU-SMI is included in the GPU driver repository. If you want to use XPU Manager, please uninstall xpu-smi and install XPU Manager. 
* Intel(R) Graphics Compute Runtime for oneAPI Level Zero (intel-level-zero-gpu and level-zero in package repositories)
* Intel(R) Graphics System Controller Firmware Update Library (intel-gsc in package repositories)
* Intel(R) Media Driver (intel-media-va-driver-non-free or intel-media in package repositories) 
* Intel(R) Media SDK Utilities (libmfx-tools or intel-mediasdk-utils in package repositories)
* Intel(R) oneVPL GPU Runtime (libmfxgen1 in package repositories)
* Intel(R) Metrics Library for MDAPI (intel-metrics-library or libigdml1 in package repositories) 
* Intel(R) Metrics Discovery Application Programming Interface (intel-metrics-discovery or libmd1 in package repositories)
 
intel-metrics-library (libigdml1) and intel-metrics-discovery (libmd1) are optional. You may use the parameter like "--force-all" to ignore them when installing Intel(R) XPU Manager.

## DEB install on Ubuntu
After adding the repository and installing the required kernel/run-time packages on [GPU Driver Installation Guides](https://dgpu-docs.intel.com/installation-guides/index.html), you may download the latest installer package from the Github release page and run apt command below to install XPU Manager and the required dependencies. 
```
sudo apt install ./xpumanager.xxxxxxxx.xxxxxx.xxxxxxxx.deb
```

## DEB uninstall
```
sudo dpkg -r xpumanager
```

## RPM install on RHEL and CentOS Stream
After importing the repository and required kernel/run-time packages on [GPU Driver Installation Guides](https://dgpu-docs.intel.com/installation-guides/index.html), you may download the latest installer package from the Github release page and run dnf command below to install XPU Manager and the required dependencies. 
```
sudo dnf install xpumanager.xxxxxxxx.xxxxxx.xxxxxxxx.rpm
```

## RPM install on SLES
After importing the repository and required kernel/run-time packages on [GPU Driver Installation Guides](https://dgpu-docs.intel.com/installation-guides/index.html), you may download the latest installer package from the Github release page and run zypper command below to install XPU Manager and the required dependencies. 
```
sudo zypper install xpumanager.xxxxxxxx.xxxxxx.xxxxxxxx.rpm
```

## Start to use XPU Manager
By default, XPU Manager is installed the folder, /usr/bin, /usr/lib and /usr/lib64. The command line tool is /usr/bin/xpumcli. Please refer to "CLI_user_guide.md" for how to use the command line tool. 

## RPM relocation install
```
rpm -i --prefix=/usr/local xpumanager.xxxxxxxx.xxxxxx.xxxxxxxx.rpm
```
You need set the environmental variable LD_LIBRARY_PATH if you change the installation folder. 

## RPM uninstall
```
sudo rpm -e xpumanager
```

## How to enable or disable some daemon monitor metrics
By default, XPU Manager has provided as many GPU metrics as possible without changing the system settings. You may follow the steps below to collect more metrics or disable some metrics. 
  
1. edit file "/lib/systemd/system/xpum.service" or "/etc/systemd/system/xpum.service" in some system.
   add "-m metric-indexes" to ExecStart. 
   Use "/usr/bin/xpumd -h" to get detailed info.  
   Sample:
   ExecStart=/usr/bin/xpumd  -p /var/xpum_daemon.pid -d /usr/lib/xpum/dump -m 0,4-38
2. Run command "sudo systemctl daemon-reload"
3. Run command "sudo systemctl restart xpum"
  
Metric types:  
  
0. Average % utilization of all GPU Engines, GPU active time of the elapsed time, per tile
1. GPU EU Array Active (%),  the normalized sum of all cycles on all EUs that were spent actively executing instructions, per tile (Disabled by default)
2. GPU EU Array Stall (%), the normalized sum of all cycles on all EUs during which the EUs were stalled. Per tile. At least one thread is loaded, but the EU is stalled, per tile. (Disabled by default)
3. GPU EU Array Idle (%), the normalized sum of all cycles on all cores when no threads were scheduled on a core. per tile.  (Disabled by default)
4. GPU Power (W), per tile
5. GPU Energy Consumed (J), per tile
6. GPU Frequency (MHz), per tile
7. GPU Core Temperature (Celsius Degree), per tile
8. GPU Memory Used (MiB)
9. GPU Memory Utilization (%), per tile
10. GPU Memory Bandwidth Utilization (%), per tile
11. GPU Memory Read (kB), per tile
12. GPU Memory Write (kB), per tile
13. GPU Memory Read Throughput(kB/s), per tile
14. GPU Memory Write Throughput(kB/s), per tile
15. GPU Compute Engine Group Utilization (%), per tile
16. GPU Media Engine Group Utilization (%), per tile
17. GPU Copy Engine Group Utilization (%), per tile
18. GPU Render Engine Group Utilization (%), per tile
19. GPU 3D Engine Group Utilization (%), per tile
20. Reset Counter, per GPU
21. Programming Errors, per tile
22. Driver Errors, per tile
23. Cache Errors Correctable, per tile
24. Cache Errors Uncorrectable, per tile
25. Display Errors Correctable, per tile (Not supported so far)
26. Display Errors Uncorrectable, per tile (Not supported so far)
27. Memory Errors Correctable, per tile
28. Memory Errors Uncorrectable, per tile
29. GPU Requested Frequency, per tile
30. GPU Memory Temperature, per tile
31. GPU Frequency Throttle Ratio, per tile (Not supported so far)
32. GPU PCIe Read Throughput (kB/s), per GPU (Disabled by default)
33. GPU PCIe Write Throughput (kB/s), per GPU (Disabled by default)
34. GPU PCIe Read (bytes), per GPU (Disabled by default)
35. GPU PCIe Write (bytes), per GPU (Disabled by default)
36. GPU Engine Utilization, per GPU engine
37. Fabric Throughput (kB/s), per tile
38. Throttle reason, per tile

### Change the system settings to enable some GPU advanced metrics
* GPU PCIe Read/Write Throughput: if these metrics are enabled, XPU Manager automatically loads MSR module by command 'modprobe msr', but XPU Manager will not automatically unload the MSR module. If you want to unload it, please run the command 'modprobe -r msr'.

### Disable some metrics to reduce the CPU usage of XPU Manager daemon
* We have tried our best to reduce the CPU usage of XPU Manager daemon. If you have many GPUs (10+) and still have concern with the CPU usage of XPU Manager daemon, you may disable some the RAS related metrics below to reduce the CPU usage further. 
    * 20. Reset Counter, per GPU
    * 21. Programming Errors, per tile
    * 22. Driver Errors, per tile
    * 23. Cache Errors Correctable, per tile
    * 24. Cache Errors Uncorrectable, per tile
    * 25. Display Errors Correctable, per tile (Not supported so far)
    * 26. Display Errors Uncorrectable, per tile (Not supported so far)
    * 27. Memory Errors Correctable, per tile
    * 28. Memory Errors Uncorrectable, per tile
 
## GPU memory ECC on/off
XPU Manager provides the GPU memory ECC on/off feature based on [IGSC](https://github.com/intel/igsc). GPU memory ECC on/off starts to work since IGSC 0.8.4. If you want to use this feature, please make sure that you install IGSC 0.8.4 or newer version.


## How to install the latest version of libcurl on CentOS 7
CentOS 7 still has the old version of libcurl. If you need update the AMC firmware through Redfish host interface, please follow the steps below to build and install libcurl.
```
yum update -y

yum install wget gcc openssl-devel make -y

wget https://curl.se/download/curl-7.56.1.tar.gz

tar xzf curl-7.56.1.tar.gz

cd curl-7.56.1

./configure --with-openssl --prefix=/usr

make 

sudo make install

curl --version
```

## XPU Manager Windows CLI tool
The Windows GPU driver version should be 31.0.101.3902 or newer. 