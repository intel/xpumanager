
# Intel XPU-SMI Installation Guide

## Requirements
* Intel GPU driver ([GPU Driver Installation Guides](https://dgpu-docs.intel.com/installation-guides/index.html))
* Intel(R) Graphics Compute Runtime for oneAPI Level Zero (intel-level-zero-gpu and level-zero in repositories)
* Intel(R) Graphics System Controller Firmware Update Library (intel-gsc in repositories)
* Intel(R) Metrics Library for MDAPI (intel-metrics-library or libigdml1 in repositories) 
* Intel(R) Metrics Discovery Application Programming Interface (intel-metrics-discovery or libmd1 in repositories)

## DEB install
sudo dpkg -i xpu-smi.1.0.0.xxxxxxxx.xxxxxx.xxxxxxxx.deb

## DEB uninstall
sudo dpkg -r xpu-smi

## RPM install
sudo rpm -i xpu-smi.1.0.0.xxxxxxxx.xxxxxx.xxxxxxxx.rpm

## RPM relocation install
rpm -i --prefix=/opt/abc xpu-smi.1.0.0.xxxxxxxx.xxxxxx.xxxxxxxx.rpm

## Start to user Intel XPU-SMI
By default, Intel XPU-SMI is installed the folder, /opt/xpum. The command line tool is xpu-smi. Please refer to "smi_user_guide.md" for how to use the command line tool. 

## RPM uninstall
sudo rpm -e xpu-smi

## RPM upgrade
sudo rpm -Uxh xpu-smi.1.0.0.xxxxxxxx.xxxxxx.xxxxxxxx.rpm

## GPU memory ECC on/off
XPU-SMI provides the GPU memory ECC on/off feature based on IGSC. GPU memory ECC on/off starts to work since IGSC 0.8.3. If you want to use this feature, please make sure that you install IGSC 0.8.3 or newer versions. 