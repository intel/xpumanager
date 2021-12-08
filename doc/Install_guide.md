
# Intel XPU Manager Installation Guide

## DEB install
sudo dpkg -i xpumanager.1.0.0.xxxxxxxx.xxxxxx.xxxxxxxx.deb

## DEB uninstall
sudo dpkg -r xpumanager

## RPM install
sudo rpm -i xpumanager.1.0.0.xxxxxxxx.xxxxxx.xxxxxxxx.rpm

## RPM relocation install
rpm -i --prefix=/opt/abc xpumanager.1.0.0.xxxxxxxx.xxxxxx.xxxxxxxx.rpm

## RPM uninstall
sudo rpm -e xpumanager

## Change daemon monitor metrics
1. edit file "/lib/systemd/system/xpum.service"
   add "-m metric-indexes" to ExecStart. 
   Use "/opt/xpum/bin/xpumd -h" to get detailed info.
   Sample:
   ExecStart=/opt/xpum/bin/xpumd -p /var/xpum_daemon.pid -d /opt/xpum/dump -m 5-8,12
2. Run command "sudo systemctl daemon-reload"
3. Run command "sudo systemctl restart xpum"