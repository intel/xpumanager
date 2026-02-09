# Intel(R) XPU System Management Interface Windows CLI User Guide
This guide describes how to use Intel(R) XPU System Management Interface Windows CLI tool to manage Intel GPU devices on Windows Server OS. The GPU driver version on Windows should be 31.0.101.3902 or newer. 
  

## Intel(R) XPU-SMI Windows CLI main features 
* Show the device info. 
* Update the GPU GFX firmware. 
* Get and change GPU configurations
* Get GPU telemetries

## Help info
Show the XPU-SMI Windows CLI tool help info. 
```
xpu-smi.exe -h
Intel XPU-SMI Command Line Interface -- v1.2 for Windows
Intel XPU-SMI Command Line Interface provides the Intel data center GPU model and monitoring capabilities. It can also be used to change the Intel data center GPU settings and update the firmware.
Intel XPU-SMI is based on Intel oneAPI Level Zero. Before using Intel XPU-SMI, the GPU driver and Intel oneAPI Level Zero should be installed rightly.

Supported devices:
- Intel Data Center GPU

Usage: xpu-smi [Options]
  xpu-smi -v
  xpu-smi -h
  xpu-smi discovery

Options:
  -h,--help                   Print this help message and exit
  -v,--version                Display version information and exit.

Subcommands:
  discovery                   Discover the GPU devices installed on this machine and provide the device info.
  updatefw                    Update GPU firmware.
  config                      Get and change the GPU settings.
  stats                       List the GPU aggregated statistics.
  dump                        Dump device statistics data.
```

Show XPU-SMI Windows CLI version and Level Zero version. 
```
xpu-smi.exe -v
CLI:
    Version: 1.2.11
    Build ID: c62f126b

Service:
    Version: 1.2.11
    Build ID: c62f126b
    Level Zero Version: 1.9.9
```

## Discover the GPU devices in this machine
Help info of the "discovery" subcommand
```
xpu-smi.exe discovery -h
Discover the GPU devices installed on this machine and provide the device info.

Usage: xpu-smi discovery [Options]
  xpu-smi discovery
  xpu-smi discovery -d [deviceId]
  xpu-smi discovery -d [deviceId] -j
  xpu-smi discovery --listamcversions

Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 Device ID to query. It will show more detailed info.

  --listamcversions           Show all AMC firmware versions. This command only works on Intel M50CYP server (BMC firmware version is 2.82 or newer).
```

Discover the devices in this machine
```
xpu-smi.exe discovery
+-----------+--------------------------------------------------------------------------------------+
| Device ID | Device Information                                                                   |
+-----------+--------------------------------------------------------------------------------------+
| 0         | Device Name: Intel(R) Data Center GPU Flex Series 170                                |
|           | Vendor Name: Intel(R) Corporation                                                    |
|           | UUID: 01000000-0000-004d-0000-000856c08086                                           |
|           | PCI BDF Address: 0000:4d:00.0                                                        |
+-----------+--------------------------------------------------------------------------------------+
```

Discover the devices in this machine and get the JSON format output
```
xpu-smi.exe discovery -j
{
    "device_list": [
        {
            "device_id": 0,
            "device_name": "Intel(R) Data Center GPU Flex Series 170",
            "device_type": "GPU",
            "pci_bdf_address": "0000:4d:00.0",
            "pci_device_id": "56c0",
            "uuid": "01000000-0000-004d-0000-000856c08086",
            "vendor_name": "Intel(R) Corporation"
        }
    ]
}
```

Show the detailed info of one device. The device info includes the model, frequency, driver/firmware info, PCI info, memory info and tile/execution unit info. 
On Windows VM with the virtual GPU(s), very limited device info can be provided. The virtual GPU memory size is retrieved with Microsoft DirectX Graphics Infrastructure API. The value may be a little different from what is shown in Windows task manager. 
```
xpu-smi.exe discovery -d 0
+-----------+--------------------------------------------------------------------------------------+
| Device ID | Device Information                                                                   |
+-----------+--------------------------------------------------------------------------------------+
| 0         | Device Type: GPU                                                                     |
|           | Device Name: Intel(R) Data Center GPU Flex Series 170                                |
|           | Vendor Name: Intel(R) Corporation                                                    |
|           | UUID: 01000000-0000-004d-0000-000856c08086                                           |
|           | Serial Number: unknown                                                               |
|           | Core Clock Rate: 2050 MHz                                                            |
|           | Stepping: unknown                                                                    |
|           |                                                                                      |
|           | Driver Version: 16999259                                                             |
|           | Firmware Name: GFX                                                                   |
|           | Firmware Version: DG02_1.3185                                                        |
|           | Firmware Name: GFX_DATA                                                              |
|           | Firmware Version: 101.651.1                                                          |
|           |                                                                                      |
|           | PCI BDF Address: 0000:4d:00.0                                                        |
|           | PCI Slot:                                                                            |
|           | PCIe Generation: 4                                                                   |
|           | PCIe Max Link Width: 16                                                              |
|           |                                                                                      |
|           | Memory Physical Size: 14336.00 MiB                                                   |
|           | Max Mem Alloc Size: 4095.99 MiB                                                      |
|           | Number of Memory Channels: 8                                                         |
|           | Memory Bus Width: 256                                                                |
|           | Max Hardware Contexts: 65536                                                         |
|           | Max Command Queue Priority:                                                          |
|           |                                                                                      |
|           | Number of EUs: 512                                                                   |
|           | Number of Tiles: 1                                                                   |
|           | Number of Slices: 8                                                                  |
|           | Number of Sub Slices per Slice: 8                                                    |
|           | Number of Threads per EU: 8                                                          |
|           | Physical EU SIMD Width: 8                                                            |
|           | Number of Media Engines:                                                             |
|           | Number of Media Enhancement Engines:                                                 |
|           |                                                                                      |
|           | Number of Xe Link ports:                                                             |
|           | Max Tx/Rx Speed per Xe Link port:                                                    |
|           | Number of Lanes per Xe Link port:                                                    |
+-----------+--------------------------------------------------------------------------------------+
```
## Update the GPU firmware
Help info of updating GPU firmware
```
xpu-smi.exe updatefw -h
Update GPU firmware.

Usage: xpu-smi updatefw [Options]
  xpu-smi updatefw -d [deviceId] -t GFX -f [imageFilePath]
  xpu-smi updatefw -d [deviceId] -t GFX_DATA -f [imageFilePath]
  xpu-smi updatefw -t AMC -f [imageFilePath]

Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 The device ID
  -t,--type                   The firmware name. Valid options: GFX, GFX_DATA, AMC. AMC firmware update just works on Intel M50CYP server (BMC firmware version is 2.82 or newer).
  -f,--file                   The firmware image file path on this server
  -y,--assumeyes              Assume that the answer to any question which would be asked is yes
  --force                     Force GFX firmware update. This parameter only works for GFX firmware.
```

Update GPU GFX firmware
```
xpu-smi.exe updatefw -d 0 -t GFX -f ATS_M150_512_C0_PVT_ES_065_gfx_fwupdate_SOC1.bin
Device 0 FW version: DG02_1.3185
Image FW version: DG02_1.3218
Do you want to continue? (y/n)
y
Start to update firmware
Firmware Name: GFX
Image path: C:\Users\xxx\xxx\ATS_M150_512_C0_PVT_ES_065_gfx_fwupdate_SOC1.bin
[============================================================] 100 %
Update firmware successfully.
```

## Get and change the GPU settings
Help info of getting/changing the GPU settings
```
xpu-smi.exe config -h
Get and change the GPU settings.

Usage: xpu-smi config [Options]
 xpu-smi config -d [deviceId]
 xpu-smi config -d [deviceId] -t [tileId] --frequencyrange [minFrequency,maxFrequency]
 xpu-smi config -d [deviceId] --powerlimit [powerValue]
 xpu-smi config -d [deviceId] --memoryecc [0|1] 0:disable; 1:enable
 xpu-smi config -d [deviceId] -t [tileId] --standby [standbyMode]
 xpu-smi config -d [deviceId] -t [tileId] --scheduler [schedulerMode]
 xpu-smi config -d [deviceId] -t [tileId] --performancefactor [engineType,factorValue]
 xpu-smi config -d [deviceId] -t [tileId] --xelinkport [portId,value]
 xpu-smi config -d [deviceId] -t [tileId] --xelinkportbeaconing [portId,value]

Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 device id
  -t,--tile                   tile id
  --frequencyrange            GPU tile-level core frequency range.
  --powerlimit                Device-level power limit.
  --memoryecc                 Enable/disable memory ECC setting. 0:disable; 1:enable
  --standby                   Tile-level standby mode. Valid options: "default"; "never".
  --scheduler                 Tile-level scheduler mode. Value options: "timeout",timeoutValue (us); "timeslice",interval (us),yieldtimeout (us);"exclusive".The valid range of all time values (us) is from 5000 to 100,000,000.
  --performancefactor         Set the tile-level performance factor. Valid options: "compute/media";factorValue. The factor value should be
                              between 0 to 100. 100 means that the workload is completely compute bounded and requires very little support from the memory support. 0 means that the workload is completely memory bounded and the performance of the memory controller needs to be increased.
  --xelinkport                Change the Xe Link port status. The value 0 means down and 1 means up.
  --xelinkportbeaconing       Change the Xe Link port beaconing status. The value 0 means off and 1 means on.
```

show the GPU settings
```
xpu-smi.exe config -d 0
+-------------+-------------------+----------------------------------------------------------------+
| Device Type | Device ID/Tile ID | Configuration                                                  |
+-------------+-------------------+----------------------------------------------------------------+
| GPU         | 0                 | Power Limit (w): 0                                             |
|             |                   |   Valid Range: 1 to 120                                        |
|             |                   |                                                                |
|             |                   | Memory ECC:                                                    |
|             |                   |   Current: enabled                                             |
|             |                   |   Pending: enabled                                             |
+-------------+-------------------+----------------------------------------------------------------+
| GPU         | 0/0               | GPU Min Frequency (MHz): 300                                   |
|             |                   | GPU Max Frequency (MHz): 2050                                  |
|             |                   |   Valid Options: 300, 317, 333, 350, 367, 383, 400, 417, 433,  |
|             |                   |     450, 467, 483, 500, 517, 533, 550, 567, 583, 600, 617,     |
|             |                   |     633, 650, 667, 683, 700, 717, 733, 750, 767, 783, 800,     |
|             |                   |     817, 833, 850, 867, 883, 900, 917, 933, 950, 967, 983,     |
|             |                   |     1000, 1017, 1033, 1050, 1067, 1083, 1100, 1117, 1133,      |
|             |                   |     1150, 1167, 1183, 1200, 1217, 1233, 1250, 1267, 1283,      |
|             |                   |     1300, 1317, 1333, 1350, 1367, 1383, 1400, 1417, 1433,      |
|             |                   |     1450, 1467, 1483, 1500, 1517, 1533, 1550, 1567, 1583,      |
|             |                   |     1600, 1617, 1633, 1650, 1667, 1683, 1700, 1717, 1733,      |
|             |                   |     1750, 1767, 1783, 1800, 1817, 1833, 1850, 1867, 1883,      |
|             |                   |     1900, 1917, 1933, 1950, 1967, 1983, 2000, 2017, 2033, 2050 |
|             |                   |                                                                |
|             |                   | Standby Mode:                                                  |
|             |                   |   Valid Options:                                               |
|             |                   |                                                                |
|             |                   | Scheduler Mode:                                                |
|             |                   |   Timeout (us):                                                |
|             |                   |   Interval (us):                                               |
|             |                   |   Yield Timeout (us):                                          |
|             |                   |                                                                |
|             |                   | Engine Type:                                                   |
|             |                   |   Performance Factor:                                          |
|             |                   | Engine Type:                                                   |
|             |                   |   Performance Factor:                                          |
|             |                   |                                                                |
|             |                   | Xe Link ports:                                                 |
|             |                   |   Up:                                                          |
|             |                   |   Down:                                                        |
|             |                   |   Beaconing On:                                                |
|             |                   |   Beaconing Off:                                               |
+-------------+-------------------+----------------------------------------------------------------+
```

Change the GPU memory ECC mode.
```
xpu-smi.exe config -d 0 --memoryecc 0
Return: Successfully disable ECC memory on GPU 0. Please reset the GPU or reboot the OS for the change to take effect.
```

Change the GPU tile core frequency range.
```
xpu-smi.exe config -d 0 -t 0 --frequencyrange 1200,1300
Return: Succeed to change the core frequency range on GPU 0 tile 0.
```
 
Change the GPU power limit.
```
xpu-smi.exe config -d 0 --powerlimit 120
Return: Succeed to set the power limit on GPU 0.
```

## Get the device real-time statistics
Help info for getting the GPU device real-time statistics 
```
xpu-smi.exe stats -h
List the GPU real-time statistics.

Usage: xpu-smi stats [Options]
  xpu-smi stats -d [deviceId]
  xpu-smi stats -d [deviceId] -j

Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 The device id to query
```

List the GPU device real-time statistics
```
xpu-smi.exe stats -d 0
+-----------------------------+--------------------------------------------------------------------+
| Device ID                   | 0                                                                  |
+-----------------------------+--------------------------------------------------------------------+
| Average % utilization of    | 0                                                                  |
| all GPU Engines             |                                                                    |
| EU Array Active (%)         |                                                                    |
| EU Array Stall (%)          |                                                                    |
| EU Array Idle (%)           |                                                                    |
|                             |                                                                    |
| Compute Engine Util (%)     | 0                                                                  |
| Render Engine Util (%)      | 0                                                                  |
| Media Engine Util (%)       | 0                                                                  |
| Decoder Engine Util (%)     |                                                                    |
| Encoder Engine Util (%)     |                                                                    |
| Copy Engine Util (%)        | 0                                                                  |
| Media EM Engine Util (%)    |                                                                    |
| 3D Engine Util (%)          |                                                                    |
+-----------------------------+--------------------------------------------------------------------+
| Reset                       |                                                                    |
| Programming Errors          |                                                                    |
| Driver Errors               |                                                                    |
| Cache Errors Correctable    |                                                                    |
| Cache Errors Uncorrectable  |                                                                    |
| Mem Errors Correctable      |                                                                    |
| Mem Errors Uncorrectable    |                                                                    |
+-----------------------------+--------------------------------------------------------------------+
| GPU Power (W)               | 16                                                                 |
| GPU Frequency (MHz)         | 1950                                                               |
| GPU Core Temperature (C)    | 52                                                                 |
| GPU Memory Temperature (C)  | 48                                                                 |
| GPU Memory Read (kB/s)      | 0                                                                  |
| GPU Memory Write (kB/s)     | 0                                                                  |
| GPU Memory Bandwidth (%)    | 0                                                                  |
| GPU Memory Used (MiB)       | 186                                                                |
| Xe Link Throughput (kB/s)   |                                                                    |
+-----------------------------+--------------------------------------------------------------------+
```

## Dump the device statistics in CSV format
For Flex series GPU on Windows OS, the supported statistics include
* GPU utilization
* GPU power
* GPU temperature
* GPU frequency
* GPU memory throughput
* GPU memory usage
* GPU engine group utilization
  
Help info of the device statistics dump. 
```
xpu-smi.exe dump -h
Dump device statistics data.

Usage: xpu-smi dump [Options]
  xpu-smi dump -d [deviceId] -t [deviceTileId] -m [metricsIds] -i [timeInterval] -n [dumpTimes]
  xpu-smi dump -d [deviceId] -t [deviceTileId] -m [metricsIds] --file [filename]

Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 The device id to query
  -t,--tile                   The device tile ID to query. If the device has only one tile, this parameter should not be specified.
  -m,--metrics                Metrics type to collect raw data, options. Separated by the comma.
                              0. Average % utilization of all GPU Engines, GPU active time of the elapsed time, per tile
                              1. GPU Power (W), per tile
                              2. GPU Frequency (MHz), per tile
                              3. GPU Core Temperature (Celsius Degree), per tile
                              4. GPU Memory Temperature (Celsius Degree), per tile
                              5. GPU Memory Utilization (%), per tile
                              6. GPU Memory Read (kB/s), per tile
                              7. GPU Memory Write (kB/s), per tile
                              8. GPU Energy Consumed (J), per tile
                              9. GPU EU Array Active (%), the normalized sum of all cycles on all EUs that were spent actively executing instructions. Per tile.
                              10. GPU EU Array Stall (%), the normalized sum of all cycles on all EUs during which the EUs were stalled. Per tile.
                                  At least one thread is loaded, but the EU is stalled. Per tile.
                              11. GPU EU Array Idle (%), the normalized sum of all cycles on all cores when no threads were scheduled on a core. Per tile.
                              12. Reset Counter, per tile.
                              13. Programming Errors, per tile.
                              14. Driver Errors, per tile.
                              15. Cache Errors Correctable, per tile.
                              16. Cache Errors Uncorrectable, per tile.
                              17. GPU Memory Bandwidth Utilization (%)
                              18. GPU Memory Used (MiB)
                              19. PCIe Read (kB/s), per GPU
                              20. PCIe Write (kB/s), per GPU
                              21. Xe Link Throughput (kB/s), a list of tile-to-tile Xe Link throughput.
                              22. Compute engine utilizations (%), per tile.
                              23. Render engine utilizations (%), per tile.
                              24. Media decoder engine utilizations (%), per tile.
                              25. Media encoder engine utilizations (%), per tile.
                              26. Copy engine utilizations (%), per tile.
                              27. Media enhancement engine utilizations (%), per tile.
                              28. 3D engine utilizations (%), per tile.
                              29. GPU Memory Errors Correctable, per tile. Other non-compute correctable errors are also included.
                              30. GPU Memory Errors Uncorrectable, per tile. Other non-compute uncorrectable errors are also included.
                              31. Compute engine group utilization (%), per tile.
                              32. Render engine group utilization (%), per tile.
                              33. Media engine group utilization (%), per tile.
                              34. Copy engine group utilization (%), per tile.
                              35. Throttle reason, per tile.

  -i                          The interval (in seconds) to dump the device statistics to screen. Default value: 1 second.
  -n                          Number of the device statistics dump to screen. The dump will never be ended if this parameter is not specified.

  --file                      Dump the required raw statistics to a file in background.
```

Dump the device statistics to screen in CSV format.
```
xpu-smi.exe dump -d 0 -m 0,1,2
Timestamp, DeviceId, Average % utilization of all GPU Engines, GPU Power (W), GPU Frequency (MHz)
05:47:42.000, 0, 0.00, 51, 2050
05:47:43.000, 0, 0.00, 50, 2050
05:47:44.000, 0, 0.00, 51, 2050
```

Dump the device statistics to csv file.
```
xpu-smi.exe dump -d 0 -m 0,1,2 --file gpudata.csv
Dump stats to file gpudata.csv. Press the key ESC to stop dumping.
ESC is pressed. Dumping is stopped.
Dumping cycle end

type gpudata.csv
Timestamp, DeviceId, Average % utilization of all GPU Engines, GPU Power (W), GPU Frequency (MHz)
06:57:15.000, 0, 0.00, 48.24, 1200
06:57:16.000, 0, 0.00, 46.47, 1200
```

## Security
The package does not require any special installation. After downloading, please check its signature and checksum. Because its execution needs the administrator privilege, it must be kept in a directory which non-admin user have no write permission. We recommend putting it under C:\Program Files.