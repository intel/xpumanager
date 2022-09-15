
# Intel XPU-SMI Installation Guide

## DEB install
sudo dpkg -i xpu-smi.1.0.0.xxxxxxxx.xxxxxx.xxxxxxxx.deb

## DEB uninstall
sudo dpkg -r xpu-smi

## RPM install
sudo rpm -i xpu-smi.1.0.0.xxxxxxxx.xxxxxx.xxxxxxxx.rpm

## Start to use Intel XPU-SMI
By default, Intel XPU-SMI is installed the folder, /opt/xpum. The command line tool is xpu-smi. Please refer to "CLI_user_guide.md" for how to use the command line tool.

## RPM relocation install
rpm -i --prefix=/opt/abc xpu-smi.1.0.0.xxxxxxxx.xxxxxx.xxxxxxxx.rpm

## RPM uninstall
sudo rpm -e xpu-smi

## GPU memory ECC on/off
XPU-SMI provides the GPU memory ECC on/off feature based on IGSC. GPU memory ECC on/off starts to work since IGSC 0.8.3. If you want to use this feature, please make sure that you install IGSC 0.8.3 or newer versions. 