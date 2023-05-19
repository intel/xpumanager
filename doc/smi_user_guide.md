# Intel(R) XPU System Management Interface User Guide
This guide describes how to use Intel(R) XPU System Management Interface to manage Intel GPU devices. 
  

## Intel(R) XPU System Management Interface main features 
* Show the device info. 
* Update the device firmware. 

## Help info
Show the XPU System Management Interface help info. 
```
xpu-smi 
Intel(R) XPU System Management Interface -- v1.0 
Intel(R) XPU System Management Interface provides the Intel data center GPU device info. It can also be used to update the firmware.  
Intel(R) XPU System Management Interface is based on Intel(R) oneAPI Level Zero. Before using XPU System Management Interface, the GPU driver and Intel(R) oneAPI Level Zero should be installed rightly.  
 
Supported devices: 
  - Intel Data Center GPU 
 
Usage: xpu-smi [Options]
  xpu-smi -v
  xpu-smi -h
  xpu-smi discovery

Options:
  -h, --help                  Print this help message and exit.
  -v, --version               Display version information and exit.

Subcommands:
  discovery                   Discover the GPU devices installed on this machine and provide the device info.
  diag                        Run some test suites to diagnose GPU.
  updatefw                    Update GPU firmware.
  topology                    get the system topology
  config                      Get and change the GPU settings.
  ps                          List status of processes.
  vgpu                        Create and remove virtual GPUs in SR-IOV configuration.
  stats                       List the GPU statistics.
  dump                        Dump device statistics data.
  log                         Collect GPU debug logs.
```
  
Show XPU System Management Interface version and Level Zero version. 
```
xpu-smi -v
CLI:
    Version: 1.0.0.20211217
    Build ID: f847c0fa

Service:
    Version: 1.0.0.20211217
    Build ID: f847c0fa
    Level Zero Version: 1.6.2
```

## Discover the devices in this machine
Help info of the "discovery" subcommand
```
xpu-smi discovery -h

Discover the GPU devices installed on this machine and provide the device info. 

Usage: xpu-smi discovery [Options]
  xpu-smi discovery
  xpu-smi discovery -d [deviceId]
  xpu-smi discovery -d [pciBdfAddress]
  xpu-smi discovery -d [deviceId] -j
  xpu-smi discovery --dump

Options:
  -h,--help                   Print this help message and exit.
  -j,--json                   Print result in JSON format
  --debug                     Print debug info

  -d,--device                 Device ID or PCI BDF address to query. It will show more detailed info.
  --pf,--physicalfunction     Display the physical functions only.
  --vf,--virtualfunction      Display the virtual functions only.
  --dump                      Property ID to dump device properties in CSV format. Separated by the comma.
                              1. Device ID
                              2. Device Name
                              3. Vendor Name
                              4. UUID
                              5. Serial Number
                              6. Core Clock Rate
                              7. Stepping
                              8. Driver Version
                              9. GFX Firmware Version
                              10. GFX Data Firmware Version
                              11. PCI BDF Address
                              12. PCI Slot
                              13. PCIe Generation
                              14. PCIe Max Link Width
                              15. OAM Socket ID
                              16. Memory Physical Size
                              17. Number of Memory Channels
                              18. Memory Bus Width
                              19. Number of EUs
                              20. Number of Media Engines
                              21. Number of Media Enhancement Engines


```



Discover the devices in this machine
```
xpu-smi discovery
+-----------+--------------------------------------------------------------------------------------+
| Device ID | Device Information                                                                   |
+-----------+--------------------------------------------------------------------------------------+
| 0         | Device Name: Intel(R) Graphics [0x020a]                                              |
|           | Vendor Name: Intel(R) Corporation                                                    |
|           | UUID: 00000000-0000-0000-0000-020a00008086                                           |
|           | PCI BDF Address: 0000:4d:00.0                                                        |
|           | Function Type: physical                                                              |
+-----------+--------------------------------------------------------------------------------------+
```

Discover the devices in this machine and get the JSON format output
```
xpu-smi discovery -j
{
    "device_list": [
        {
            "device_id": 0,
            "device_name": "Intel(R) Graphics [0x020a]",
            "device_type": "GPU",
            "pci_bdf_address": "0000:4d:00.0",
            "pci_device_id": "0x20a",
            "uuid": "00000000-0000-0000-0000-020a00008086",
            "vendor_name": "Intel(R) Corporation"
        }
    ]
}
```

Show the detailed info of one device. The device info includes the model, frequency, driver/firmware info, PCI info, memory info and tile/execution unit info. 
```
xpu-smi discovery -d 0000:4d:00.0
+-----------+--------------------------------------------------------------------------------------+
| Device ID | Device Information                                                                   |
+-----------+--------------------------------------------------------------------------------------+
| 0         | Device Type: GPU                                                                     |
|           | Device Name: Intel(R) Graphics [0x020a]                                              |
|           | Vendor Name: Intel(R) Corporation                                                    |
|           | UUID: 00000000-0000-0000-0000-020a00008086                                           |
|           | Serial Number: unknown                                                               |
|           | Core Clock Rate: 1400 MHz                                                            |
|           | Stepping: B1                                                                         |
|           |                                                                                      |
|           | Driver Version: 16929133                                                             |
|           | Firmware Name: GFX                                                                   |
|           | Firmware Version: ATS0_1.1                                                           |
|           |                                                                                      |
|           | PCI BDF Address: 0000:4d:00.0                                                        |
|           | PCI Slot: Riser 1, slot 1                                                            |
|           | PCIe Generation: 4                                                                   |
|           | PCIe Max Link Width: 16                                                              |
|           |                                                                                      |
|           | Memory Physical Size: 32768.00 MiB                                                   |
|           | Max Mem Alloc Size: 4095.99 MiB                                                      |
|           | ECC State: enabled                                                                   |
|           | Number of Memory Channels: 2                                                         |
|           | Memory Bus Width: 128                                                                |
|           | Max Hardware Contexts: 65536                                                         |
|           | Max Command Queue Priority: 0                                                        |
|           |                                                                                      |
|           | Number of EUs: 512                                                                   |
|           | Number of Tiles: 2                                                                   |
|           | Number of Slices: 2                                                                  |
|           | Number of Sub Slices per Slice: 30                                                   |
|           | Number of Threads per EU: 8                                                          |
|           | Physical EU SIMD Width: 8                                                            |
|           | Number of Media Engines: 2                                                           |
|           | Number of Media Enhancement Engines: 2                                               |
|           |                                                                                      |
|           | Number of Xe Link ports: 16                                                          |
|           | Max Tx/Rx Speed per Xe Link port: 51879.88 MiB/s                                     |
|           | Number of Lanes per Xe Link port: 4                                                  |
+-----------+--------------------------------------------------------------------------------------+
```

Dump the specified GPU device info to csv format. 
```
xpu-smi discovery --dump 1,2,3,6
Device ID,Device Name,Vendor Name,Core Clock Rate
0,"Intel(R) Graphics [0x56c0]","Intel(R) Corporation","2050 MHz"
1,"Intel(R) Graphics [0x56c0]","Intel(R) Corporation","2050 MHz"

```

## Get the system topology
Help info of get the system topology
```
xpu-smi topology

Get the system topology info.

Usage: xpu-smi topology [Options]
  xpu-smi topology -d [deviceId]
  xpu-smi topology -d [pciBdfAddress]
  xpu-smi topology -d [deviceId] -j
  xpu-smi topology -f [filename]
  xpu-smi topology -m

optional arguments:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 The device ID or PCI BDF address to query
  -f,--file                   Generate the system topology with the GPU info to a XML file. 
  -m,--matrix                 Print the CPU/GPU topology matrix. 
                                S: Self
                                XL[laneCount]: Two tiles on the different cards are directly connected by Xe Link.  Xe Link lane count is also provided.
                                XL*: Two tiles on the different cards are connected by Xe Link + MDF. They are not directly connected by Xe Link. 
                                SYS: Connected with PCIe between NUMA nodes
                                NODE: Connected with PCIe within a NUMA node
                                MDF: Connected with Multi-Die Fabric Interface
```

Get the hardware topology info which is related to the GPU
```
xpu-smi topology -d 0
+-----------+--------------------------------------------------------------------------------------+
| Device ID | Topology Information                                                                 |
+-----------+--------------------------------------------------------------------------------------+
| 0         | Local CPU List: 0-23,48-71                                                           |
|           | Local CPUs: 000000ff,ffff0000,00ffffff                                               |
|           | PCIe Switch Count: 1                                                                 |
|           | PCIe Switch: /sys/devices/pci0000:4a/0000:4a:02.0/0000:4b:00.0/0000:4c:00.0          |
|           |   /0000:4d:00.0/0000:4e:18.0/0000:54:00.0/0000:55:01                                 |
+-----------+--------------------------------------------------------------------------------------+
```
  
Generate the system hardware topology to a XML file. 
```
xpu-smi topology -f topo.xml
The system topology is generated to the file topo.xml. 
```

Generate the CPU/GPU topology matrix. 
```
xpu-smi topology -m

         GPU 0/0|GPU 0/1|GPU 1/0|GPU 1/1|CPU Affinity
GPU 0/0 |S      |MDF    |XL*    |XL16   |0-23,48-71
GPU 0/1 |MDF    |S      |XL16   |XL*    |0-23,48-71
GPU 1/0 |XL*    |XL16   |S      |MDF    |24-47,72-95
GPU 1/1 |XL16   |XL*    |MDF    |S      |24-47,72-95
```
  
  
## Update the GPU firmware
Help info of updating GPU firmware
```
xpu-smi updatefw

Update GPU firmware.

Usage: xpu-smi updatefw [Options]
  xpu-smi updatefw -d [deviceId] -t GFX -f [imageFilePath]
  xpu-smi updatefw -d [pciBdfAddress] -t GFX -f [imageFilePath]

Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 The device ID or PCI BDF address
  -t,--type                   The firmware name. Valid options: GFX, GFX_DATA.
  -f,--file                   The firmware image file path on this server.
```

Update GPU GFX firmware
```
xpu-smi updatefw -d 0 -t GFX -f /home/test/tools/ATS_M3.bin
This GPU card has multiple cores. This operation will update all firmware. Do you want to continue? (y/n) y
Start to update firmware:
Firmware name: GFX
Image path: /home/test/tools/ATS_M3.bin
Update firmware successfully. Please reboot OS to take effect. 
```
  
## Diagnose GPU with different test suites
When running tests on GPU, GPU will be used exclusively. There will be obviously performance impact on the GPU. Some CPU performance may also be impacted. 

Help info for GPU diagnostics
```
xpu-smi diag

Run some test suites to diagnose GPU.

Usage: xpu-smi diag [Options]
  xpu-smi diag -d [deviceId] -l [level]
  xpu-smi diag -d [pciBdfAddress] -l [level]
  xpu-smi diag -d [deviceId] -l [level] -j
  xpu-smi diag -d [deviceId] --singletest [testIds]
  xpu-smi diag -d [pciBdfAddress] --singletest [testIds]
  xpu-smi diag -d [deviceId] --singletest [testIds] -j
  xpu-smi diag -d [pciBdfAddress] --singletest [testIds] -j
  xpu-smi diag -d [deviceIds] --stress
  xpu-smi diag -d [deviceIds] --stress --stresstime [stress time]
  xpu-smi diag --precheck
  xpu-smi diag --precheck -j
  xpu-smi diag --precheck --gpu
  xpu-smi diag --precheck --gpu -j
  xpu-smi diag --stress
  xpu-smi diag --stress --stresstime [time]

  
Options:
  -h,--help                   Print this help message and exit.
  -j,--json                   Print result in JSON format.

  -d,--device                 The device IDs or PCI BDF addresses. -1 means all devices
  -l,--level                  The diagnostic levels to run. The valid options include
                                1. quick test
                                2. medium test - this diagnostic level will have the significant performance impact on the specified GPUs
                                3. long test - this diagnostic level will have the significant performance impact on the specified GPUs
  --stress                    Stress the GPU(s) for the specified time
  --stresstime                Stress time (in minutes). It is an optional parameter. If not specified, CLI does the stress until manually stopped.
  --precheck                  Do the precheck on the GPU and GPU driver
```

Run test to diagnose GPU
```
xpu-smi diag -d 0 -l 1
Device Type: GPU
+------------------------+-------------------------------------------------------------------------+
| Device ID              | 0                                                                       |
+------------------------+-------------------------------------------------------------------------+
| Level                  | 1                                                                       |
| Result                 | Fail                                                                    |
| Items                  | 4                                                                       |
+------------------------+-------------------------------------------------------------------------+
| Software Env Variables | Result: Pass                                                            |
|                        | Message: Pass to check environment variables.                           |
+------------------------+-------------------------------------------------------------------------+
| Software Library       | Result: Pass                                                            |
|                        | Message: Pass to check libraries                                        |
+------------------------+-------------------------------------------------------------------------+
| Software Permission    | Result: Pass                                                            |
|                        | Message: Pass to check permission                                       |
+------------------------+-------------------------------------------------------------------------+
| Software Exclusive     | Result: Fail                                                            |
|                        | Message: Fail to check the software exclusive. 2 process(es) are        |
|                        |   using the device.                                                     |
|                        |   PID: 633972, Command: ./ze_gemm                                       |
|                        |   PID: 633973, Command: ./ze_gemm                                       |
+------------------------+-------------------------------------------------------------------------+

```

Check the GPU status
```
xpu-smi diag --precheck
+------------------+-------------------------------------------------------------------------------+
| Component        | Status                                                                        |
+------------------+-------------------------------------------------------------------------------+
| GPU              | 2 (0x56c1)                                                                    |
| Driver           | Pass                                                                          |
| GPU Status       | Pass                                                                          |
| CPU Status       | Pass                                                                          |
+------------------+-------------------------------------------------------------------------------+
```

Stress the GPU
```
xpu-smi diag --stress
Stress on all GPU
Device: 0 Finished:0 Time: 0 seconds
Device: 1 Finished:0 Time: 0 seconds
Device: 0 Finished:0 Time: 5 seconds
Device: 1 Finished:0 Time: 5 seconds
^C
```

Run the particular tests on the specified GPU
```
xpu-smi diag -d 0 --singletest 1,4
+-------------------------+------------------------------------------------------------------------+
| Device ID               | 0                                                                      |
+-------------------------+------------------------------------------------------------------------+
| Performance Computation | Result: Pass                                                           |
|                         | Message: Pass to check computation performance. Its single-precision   |
|                         |   GFLOPS is 10938.459.                                                 |
+-------------------------+------------------------------------------------------------------------+
| Media Codec             | Result: Pass                                                           |
|                         | Message: Pass to check Media transcode performance.                    |
|                         |  1080p H.265 : 474 FPS                                                 |
|                         |  1080p H.264 : 430 FPS                                                 |
|                         |  4K H.265 : 139 FPS                                                    |
|                         |  4K H.264 : 119 FPS                                                    |
+-------------------------+------------------------------------------------------------------------+
```

## Get and change the GPU settings
Help info of getting/changing the GPU settings
```
xpu-smi config

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
  xpu-smi config -d [deviceId] --reset
  
  
Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 The device ID or PCI BDF address to query
  -t,--tile                   The tile ID

  --frequencyrange            GPU tile-level core frequency range.
  --powerlimit                GPU-level power limit. 
  --standby                   Tile-level standby mode. Valid options: "default"; "never".
  --scheduler                 Tile-level scheduler mode. Value options: "timeout",timeoutValue (us); 
                                "timeslice",interval (us),yieldtimeout (us); "exclusive". The valid range of all time values (us) is from 5000 to 100,000,000.
  --performancefactor         Set the tile-level performance factor. Valid options: "compute/media";factorValue. The factor value should be 
                                between 0 to 100. 100 means that the workload is completely compute bounded and requires very little support from the memory support. 0 means that the workload is completely memory bounded and the performance of the memory controller needs to be increased. 
  --xelinkport                Change the Xe Link port status. The value 0 means down and 1 means up.
  --xelinkportbeaconing       Change the Xe Link port beaconing status. The value 0 means off and 1 means on.
  --memoryecc                 Enable/disable memory ECC setting. 0:disable; 1:enable
  --reset                     Reset device by SBR (Secondary Bus Reset).

```

show the GPU settings
```
xpu-smi config -d 0
+-------------+-------------------+----------------------------------------------------------------+
| Device Type | Device ID/Tile ID | Configuration                                                  |
+-------------+-------------------+----------------------------------------------------------------+
| GPU         | 0                 | Power Limit (w): 300.0                                         |
|             |                   |   Valid Range: 1 to 500                                        |
|             |                   |                                                                |
|             |                   | Memory ECC:                                                    |
|             |                   |   Current: enabled                                             |
|             |                   |   Pending: enabled                                             |
+-------------+-------------------+----------------------------------------------------------------+
| GPU         | 0/0               | GPU Min Frequency(MHz): 300.0                                  |
|             |                   | GPU Max Frequency(MHz): 1300.0                                 |
|             |                   |   Valid Options: 300, 350, 400, 450, 500,550, 600, 650, 700    |
|             |                   |       750, 800, 850, 900, 950, 1000, 1050, 1100, 1150, 1200    |
|             |                   |       1250, 1300                                               |
|             |                   |                                                                |
|             |                   | Standby Mode: default                                          |
|             |                   |   Valid Options: default, never                                |
|             |                   |                                                                |
|             |                   | Scheduler Mode: timeslice                                      |
|             |                   |   Interval(us): 5000                                           |
|             |                   |   Yield Timeout (us): 640000                                   |
|             |                   |                                                                |
|             |                   | Engine Type: compute                                           |
|             |                   |   Performance Factor: 70                                       |
|             |                   | Engine Type: media                                             |
|             |                   |   Performance Factor: 50                                       |
|             |                   |                                                                |
|             |                   | Xe Link ports:                                                 |
|             |                   |   Up: 0,1,2,3                                                  |
|             |                   |   Down: 4,5,6,7                                                |
|             |                   |   Beaconing On: 0,1,2,3                                        |
|             |                   |   Beaconing Off: 4,5,6,7                                       |
+-------------+-------------------+----------------------------------------------------------------+
| GPU         | 0/1               | GPU Min Frequency(MHz): 300.0                                  |
|             |                   | GPU Max Frequency(MHz): 1300.0                                 |
|             |                   |   Valid Options: 300, 350, 400, 450, 500,550, 600, 650, 700    |
|             |                   |     750, 800, 850, 900, 950, 1000, 1050, 1100, 1150, 1200      |
|             |                   |     1250, 1300                                                 |
|             |                   |                                                                |
|             |                   | Standby Mode: default                                          |
|             |                   |   Valid Options: default, never                                |
|             |                   |                                                                |
|             |                   | Scheduler Mode: timeslice                                      |
|             |                   |   Interval(us): 5000                                           |
|             |                   |   Yield Timeout (us): 640000                                   |
|             |                   |                                                                |
|             |                   | Engine Type: compute                                           |
|             |                   |   Performance Factor: 70                                       |
|             |                   | Engine Type: media                                             |
|             |                   |   Performance Factor: 50                                       |
|             |                   |                                                                |
|             |                   | Xe Link ports:                                                 |
|             |                   |   Up: 0,1,2,3                                                  |
|             |                   |   Down: 4,5,6,7                                                |
|             |                   |   Beaconing On: 0,1,2,3                                        |
|             |                   |   Beaconing Off: 4,5,6,7                                       |
+-------------+-------------------+----------------------------------------------------------------+
```
 
Change the GPU tile core frequency range.
```
xpu-smi config -d 0 -t 0 --frequencyrange 1200,1300
Succeed to change the core frequency range on GPU 0 tile 0.
```
 
Change the GPU power limit.
```
xpu-smi config -d 0 --powerlimit 299
Succeed to set the power limit on GPU 0.
```

Change the GPU memory ECC mode.
```
xpu-smi config -d 0 --memoryecc 0
Return: Succeed to change the ECC mode to be disabled on GPU 0. Please reset GPU or reboot OS to take effect.
```
 
Change the GPU tile standby mode.
```
xpu-smi config -d 0 -t 0 --standby never
Succeed to change the standby mode on GPU 0.
```

Change the GPU tile scheduler mode.
```
xpu-smi config -d 0 -t 0 --scheduler timeout,640000
Succeed to change the scheduler mode on GPU 0 tile 0.
```
  
Set the performance factor
```
xpu-smi config -d 0 -t 0 --performancefactor compute,70
Succeed to change the compute performance factor to 70 on GPU 0 tile 0.
```

Change the Xe Link port status
```
xpu-smi config -d 0 -t 0 --xelinkport 0,1
Succeed to change Xe Link port 0 to up.
```

Change the Xe Link port beaconing status
```
xpu-smi config -d 0 -t 0 --xelinkportbeaconing 0,1
Succeed to change Xe Link port 0 beaconing to on.
```


## Get the device real-time statistics
Help info for getting the GPU device real-time statistics 
```
List the GPU statistics.

Usage: xpu-smi stats [Options]
  xpu-smi stats
  xpu-smi stats -d [deviceId]
  xpu-smi stats -d [pciBdfAddress]
  xpu-smi stats -d [deviceId] -j
  xpu-smi stats -d [pciBdfAddress] -j
  xpu-smi stats -d [deviceId] -e
  xpu-smi stats -d [pciBdfAddress] -e
  xpu-smi stats -d [deviceId] -e -j
  xpu-smi stats -d [pciBdfAddress] -e -j

Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format
  --debug                     Print debug info

  -d,--device                 The device ID or PCI BDF address to query
  -e,--eu                     Show the EU metrics
```
 
List the GPU device real-time statistics that are collected by xpu-smi
```
xpu-smi stats -d 0
+-----------------------------+--------------------------------------------------------------------+
| Device ID                   | 0                                                                  |
+-----------------------------+--------------------------------------------------------------------+
| GPU Utilization (%)         | 0                                                                  |
| EU Array Active (%)         |                                                                    |
| EU Array Stall (%)          |                                                                    |
| EU Array Idle (%)           |                                                                    |
|                             |                                                                    |
| Compute Engine Util (%)     | 0; Engine 0: 0                                                     |
| Render Engine Util (%)      | 0; Engine 0: 0                                                     |
| Media Engine Util (%)       | 0                                                                  |
| Decoder Engine Util (%)     | Engine 0: 0, Engine 1: 0                                           |
| Encoder Engine Util (%)     | Engine 0: 0, Engine 1: 0                                           |
| Copy Engine Util (%)        | 0; Engine 0: 0                                                     |
| Media EM Engine Util (%)    | Engine 0: 0, Engine 1: 0                                           |
| 3D Engine Util (%)          |                                                                    |
+-----------------------------+--------------------------------------------------------------------+
| Reset                       | 0                                                                  |
| Programming Errors          | 0                                                                  |
| Driver Errors               | 0                                                                  |
| Cache Errors Correctable    | 0                                                                  |
| Cache Errors Uncorrectable  | 0                                                                  |
| Mem Errors Correctable      | 0                                                                  |
| Mem Errors Uncorrectable    | 0                                                                  |
+-----------------------------+--------------------------------------------------------------------+
| GPU Power (W)               | 15                                                                 |
| GPU Frequency (MHz)         | 0                                                                  |
| GPU Core Temperature (C)    | 41                                                                 |
| GPU Memory Temperature (C)  |                                                                    |
| GPU Memory Read (kB/s)      |                                                                    |
| GPU Memory Write (kB/s)     |                                                                    |
| GPU Memory Bandwidth (%)    |                                                                    |
| GPU Memory Used (MiB)       | 24                                                                 |
| Xe Link Throughput (kB/s)   |                                                                    |
+-----------------------------+--------------------------------------------------------------------+
```
  
## Dump the device statistics in CSV format
Help info of the device statistics dump. Please note that the metrics 'Programming Errors', 'Driver Errors', 'Cache Errors Correctable' and 'Cache Errors Uncorrectable' are not implemented in dump sub-command so far. Please do not dump these metrics. 
```
xpu-smi dump -h
Dump device statistics data.

Usage: xpu-smi dump [Options]
  xpu-smi dump -d [deviceIds] -m [metricsIds] -i [timeInterval] -n [dumpTimes]
  xpu-smi dump -d [deviceIds] -t [deviceTileIds] -m [metricsIds] -i [timeInterval] -n [dumpTimes]
  xpu-smi dump -d [pciBdfAddress] -t [deviceTileIds] -m [metricsIds] -i [timeInterval] -n [dumpTimes]

Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format
  --debug                     Print debug info

  -d,--device                 The device IDs or PCI BDF addresses to query. The value of "-1" means all devices.
  -t,--tile                   The device tile ID to query. If the device has only one tile, this parameter should not be specified.
  -m,--metrics                Metrics type to collect raw data, options. Separated by the comma.
                              0. GPU Utilization (%), GPU active time of the elapsed time, per tile
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
                              31. Compute engine group utilization (%), per tile
                              32. Render engine group utilization (%), per tile
                              33. Media engine group utilization (%), per tile
                              34. Copy engine group utilization (%), per tile
                              35. Throttle reason, per tile

  -i                          The interval (in seconds) to dump the device statistics to screen. Default value: 1 second.
  -n                          Number of the device statistics dump to screen. The dump will never be ended if this parameter is not specified.

```

Dump the device statistics to screen in CSV format.
```
xpu-smi dump -d 0 -m 0,1,2 -i 1 -n 5
Timestamp, DeviceId, GPU Utilization (%), GPU Power (W), GPU Frequency (MHz)
06:14:46.000,    0, 0.00, 14.61,    0
06:14:47.000,    0, 0.00, 14.59,    0
06:14:48.000,    0, 0.00, 14.61,    0
06:14:49.000,    0, 0.00, 14.61,    0
06:14:50.000,    0, 0.00, 14.61,    0
```
 
## Collect the GPU debug log files
Help info of collecting GPU log files.  
```
xpu-smi log -h
Collect GPU debug logs.

Usage: xpu-smi log [Options]
 xpu-smi log -f [tarGzipFileName]

Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -f,--file                   The file (a tar.gz) to archive all the debug logs
```
 
Collect the GPU log files.
```
xpu-smi log -f 1217.tar.gz
Done
```
 
## Set up GPU SR-IOV configuration
This GPU SR-IOV feature only works for Intel Flex series GPUs. Only Ubuntu and CentOS 7 are validated. 
 
### BIOS settings for GPU SR-IOV
#### Intel M50CYP
Advanced -> PCI Configuration -> Memory Mapped I/O above 4GB : Enabled  
Advanced -> PCI Configuration -> MMIO High Base : 56T  
Advanced -> PCI Configuration -> Memory Mapped I/O Size : 1024G  
Advanced -> System Acoustic and Performance Configuration -> Set Fan Profile : Performance  
Advanced -> Integrated IO Configuration -> PCIe Misc. Configuration -> PCIe ASPM Support (Global) : Disabled  
Advanced -> Power & Performance -> Workload Configuration : I/O Sensitive  
Advanced -> Power & Performance -> CPU P State Control -> Intel(R) Turbo Boost Technology : Enabled  
Advanced -> PCI Configuration -> SR-IOV Support : Enabled  
Advanced -> Processor Configuration -> Intel(R) Virtualization Technology: Enabled  
Advanced -> Integrated IO Configuration -> Intel(R) VT for Directed I/O : Enabled  
  
#### Inspur NF5280M6
MMCFG Base = Auto  
MMCFG Size = Auto  
MMIO High Base = 32T  
MMIO High Granularity Size = 1024G  
VMX = Enabled  
SR-IOV Support = Enabled  
Intel VT for Directed I/O (VT-d) = Enabled  
Power/Performance Profile = Virtualization  
  
#### H3C R5300 G5
 MMCFG Base=Auto  
 MMIO High Base=32T  
 MMIO High Granularity Size=1024G  
 VMX=Enabled  
 SR-IOV Support=Enabled  
 Intel    VT for Directed I/O=Enabled  
 ENERGY_PERF_BIAS_CFG mode=Performance  
 Workload Configuration=Balanced  
 Workload Profile Configuration=Graphic Processing  
    
#### Dell PowerEdge XR12
 Integrated Devices -> Memory Mapped I/O above 4GB = Enabled  
 Integrated Devices -> Memory Mapped Base = 56TB  
 Integrated Devices -> SR-IOV Global Enable = Enabled  
 Processor Settings -> Virtualization Technology = Enabled  
 System Profile Settings -> System Profile = Performance  
  
### Add Linux kernel command line options
After BIOS settings are rightly configured and the GPU driver is installed, some kernel command line options need be added. They are "intel_iommu=on i915.max_vfs=31". "intel_iommmu" is for IOMMU and "i915.max_vfs" is for SR-IOV. After you set them, please reboot OS to take effect. You may check the content of /proc/cmdline to confirm that they are rightly set. 
  
  
### Help info of GPU SR-IOV configuration feature. 
```
xpu-smi vgpu
Create and remove virtual GPUs in SR-IOV configuration.

Usage: xpu-smi vgpu [Options]
 xpu-smi vgpu --precheck
 xpu-smi vgpu -d [deviceId] -c -n [vGpuNumber] --lmem [vGpuMemorySize]
 xpu-smi vgpu -d [pciBdfAddress] -c -n [vGpuNumber] --lmem [vGpuMemorySize]
 xpu-smi vgpu -d [deviceId] -r
 xpu-smi vgpu -d [pciBdfAddress] -r
 xpu-smi vgpu -d [deviceId] -l
 xpu-smi vgpu -d [pciBdfAddress] -l

Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  --precheck                  Check if BIOS settings and kernel command line options are ready to create virtual GPUs

  -d,--device                 Device ID or PCI BDF address
  -c,--create                 Create the virtual GPUs
  -n                          The number of virtual GPUs to create
  --lmem                      The memory size of each virtual GPUs, in MiB

  -l,--list                   List all virtual GPUs on the specified physical GPU

  -r,--remove                 Remove all virtual GPUs on the specified physical GPU
  -y,--assumeyes              Assume that the answer to any question which would be asked is yes
```
 
### GPU SR-IOV precheck
```
 xpu-smi vgpu --precheck
+------------------+-------------------------------------------------------------------------------+
| VMX Flag         | Result: Pass                                                                  |
|                  | Message:                                                                      |
+------------------+-------------------------------------------------------------------------------+
| SR-IOV           | Status: Fail                                                                  |
|                  | Message: SR-IOV is disabled. Please set the related BIOS settings and         |
|                  |   kernel command line options.                                                |
+------------------+-------------------------------------------------------------------------------+
| IOMMU            | Status: Pass                                                                  |
|                  | Message:                                                                      |
+------------------+-------------------------------------------------------------------------------+
```
 
### Create the virtual GPUs
After all GPU SR-IOV pre-checks are passed, you may create the virtual GPUs
```
sudo xpu-smi vgpu -d 0 -c -n 4 --lmem 500M
+--------------------------------------------------------------------------------------------------+
| Device Information                                                                               |
+--------------------------------------------------------------------------------------------------+
| PCI BDF Address: 0000:4d:00.0                                                                    |
| Function Type: physical                                                                          |
| Memory Physical Size: 14260.97 MiB                                                               |
+--------------------------------------------------------------------------------------------------+
| PCI BDF Address: 0000:4d:00.1                                                                    |
| Function Type: virtual                                                                           |
| Memory Physical Size: 500.00 MiB                                                                 |
+--------------------------------------------------------------------------------------------------+
| PCI BDF Address: 0000:4d:00.2                                                                    |
| Function Type: virtual                                                                           |
| Memory Physical Size: 500.00 MiB                                                                 |
+--------------------------------------------------------------------------------------------------+
| PCI BDF Address: 0000:4d:00.3                                                                    |
| Function Type: virtual                                                                           |
| Memory Physical Size: 500.00 MiB                                                                 |
+--------------------------------------------------------------------------------------------------+
| PCI BDF Address: 0000:4d:00.4                                                                    |
| Function Type: virtual                                                                           |
| Memory Physical Size: 500.00 MiB                                                                 |
+--------------------------------------------------------------------------------------------------+
```

### list the virtual GPUs
```
sudo xpu-smi vgpu -d 0 -l
+--------------------------------------------------------------------------------------------------+
| Device Information                                                                               |
+--------------------------------------------------------------------------------------------------+
| PCI BDF Address: 0000:4d:00.0                                                                    |
| Function Type: physical                                                                          |
| Memory Physical Size: 14260.97 MiB                                                               |
+--------------------------------------------------------------------------------------------------+
| PCI BDF Address: 0000:4d:00.1                                                                    |
| Function Type: virtual                                                                           |
| Memory Physical Size: 500.00 MiB                                                                 |
+--------------------------------------------------------------------------------------------------+
| PCI BDF Address: 0000:4d:00.2                                                                    |
| Function Type: virtual                                                                           |
| Memory Physical Size: 500.00 MiB                                                                 |
+--------------------------------------------------------------------------------------------------+
| PCI BDF Address: 0000:4d:00.3                                                                    |
| Function Type: virtual                                                                           |
| Memory Physical Size: 500.00 MiB                                                                 |
+--------------------------------------------------------------------------------------------------+
| PCI BDF Address: 0000:4d:00.4                                                                    |
| Function Type: virtual                                                                           |
| Memory Physical Size: 500.00 MiB                                                                 |
+--------------------------------------------------------------------------------------------------+
```

### Remove all the virtual GPUs on the specified GPU
```
sudo xpu-smi vgpu -d 0 -r
CAUTION: we are removing all VFs on device 0, please make sure all VF-assigned virtual machines are shut down.
Please confirm to proceed (y/n) y
All virtual GPUs on the device 0 are removed.

```

### Limitations
 * XPU manager (in Host OS Linux) cannot discover and monitor a VF if it is assigned to an active VM guest or sriov_drivers_autoprobe is set to 0. If only 1 VF was created, end users may understand VF utilizations by looking at metrics of PF. 
 * XPU manager (in Guest OS Windows) can only monitor GPU utilization, other metrics are not available