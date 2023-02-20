
# Intel(R) XPU-SMI Installation Guide

## Requirements
* Intel GPU driver ([GPU Driver Installation Guides](https://dgpu-docs.intel.com/installation-guides/index.html))
* Intel(R) Graphics Compute Runtime for oneAPI Level Zero (intel-level-zero-gpu and level-zero in package repositories)
* Intel(R) Graphics System Controller Firmware Update Library (intel-gsc in package repositories)
* Intel(R) Media Driver (intel-media-va-driver-non-free or intel-media in package repositories) 
* Intel(R) Media SDK Utilities (libmfx-tools or intel-mediasdk-utils in package repositories)
* Intel(R) oneVPL GPU Runtime (libmfxgen1 in package repositories)
* Intel(R) Metrics Library for MDAPI (intel-metrics-library or libigdml1 in package repositories) 
* Intel(R) Metrics Discovery Application Programming Interface (intel-metrics-discovery or libmd1 in package repositories)
 
intel-metrics-library (libigdml1) and intel-metrics-discovery (libmd1) are optional. You may use the parameter like "--force-all" to ignore them when installing XPU-SMI.


## DEB install on Ubuntu
After adding the repository and installing the required kernel/run-time packages on [GPU Driver Installation Guides](https://dgpu-docs.intel.com/installation-guides/index.html), you may run apt command below to install XPU-SMI and the required dependencies. 
```
sudo apt install ./xpu-smi.xxxxxxxx.xxxxxx.xxxxxxxx.deb
```

## DEB uninstall
```
sudo dpkg -r xpu-smi
```

## RPM install on RHEL and CentOS Stream
After importing the repository and required kernel/run-time packages on [GPU Driver Installation Guides](https://dgpu-docs.intel.com/installation-guides/index.html), you may run dnf command below to install XPU-SMI and the required dependencies. 
```
sudo dnf install xpu-smi.xxxxxxxx.xxxxxx.xxxxxxxx.rpm
```

## RPM install on SLES
After importing the repository and required kernel/run-time packages on [GPU Driver Installation Guides](https://dgpu-docs.intel.com/installation-guides/index.html), you may run zypper command below to install XPU-SMI and the required dependencies. 
```
sudo zypper install xpu-smi.xxxxxxxx.xxxxxx.xxxxxxxx.rpm
```

## RPM relocation install
```
rpm -i --prefix=/usr/local xpu-smi.xxxxxxxx.xxxxxx.xxxxxxxx.rpm
```
You need set the environmental variable LD_LIBRARY_PATH if you change the installation folder. 

## Start to use XPU-SMI
By default, XPU-SMI is installed the folder, /usr/bin, /usr/lib and /usr/lib64. The command line tool is xpu-smi. Please refer to "smi_user_guide.md" for how to use the command line tool. 

## RPM uninstall
```
sudo rpm -e xpu-smi
```

## RPM upgrade
```
sudo rpm -Uxh xpu-smi.xxxxxxxx.xxxxxx.xxxxxxxx.rpm
```

## GPU memory ECC on/off
XPU-SMI provides the GPU memory ECC on/off feature based on IGSC. GPU memory ECC on/off starts to work since IGSC 0.8.3. If you want to use this feature, please make sure that you install IGSC 0.8.3 or newer versions. 