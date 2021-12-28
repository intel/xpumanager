
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
  
Metric types:  
  
0. GPU Utilizatio (%), GPU active time of the elapsed time, per tile
1. GPU EU Array Active (%),  the normalized sum of all cycles on all EUs that were spent actively executing instructions, per tile. (Disabled by default)
2. GPU EU Array Stall (%), the normalized sum of all cycles on all EUs during which the EUs were stalled. Per tile. At least one thread is loaded, but the EU is stalled, per tile. (Disabled by default)
3. GPU EU Array Idle (%), the normalized sum of all cycles on all cores when no threads were scheduled on a core. per tile.  (Disabled by default)
4. GPU Power (W), per tile
5. GPU Energy Consumed (J), per tile
6. GPU Frequency (MHz), per tile
7. GPU Core Temperature (Celsius Degree), per tile
8. GPU Memory Used (MiB)
9. GPU Memory Utilization (%), per tile
10. GPU Memory Bandwidth Utilization. (%), per tile
11. GPU Memory Read (kB), per tile
12. GPU Memory Write (kB), per tile
13. GPU Memory Read Throughput(kB/s), per tile
14. GPU Memory Write Throughput(kB/s), per tile
15. GPU Compute Engine Group Utilization (%), per tile
16. GPU Media Engine Group Utilization (%), per tile
17. GPU Copy Engine Group Utilization (%), per tile
18. GPU Render Engine Group Utilization (%), per tile
19. GPU 3D Engine Group Utilization (%), per tile
20. Reset Counter, per GPU.
21. Programming Errors, per tile.
22. Driver Errors, per tile.
23. Cache Errors Correctable, per tile.
24. Cache Errors Uncorrectable, per tile.
25. Display Errors Correctable, per tile. (Not supported so far)
26. Display Errors Uncorrectable, per tile. (Not supported so far)
27. GPU Requsted Frequency, per tile
28. GPU Memory Temperature, per tile
29. GPU Frequency Throttle Ratio, per tile. (Not supported so far)

Please note: if you want to collect the metrics, GPU EU Array Active/Stall/Idle, please set the value of /proc/sys/dev/i915/perf_stream_paranoid to 0. Or else, XPUM daemon can't be started. 