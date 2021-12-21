
# Intel XPU Manager Command Line Interface User Guide
This guide describes how to use Intel XPU Manager Command Line Interface to manage Intel GPU devices. 
  

## Intel XPU Manager Command Line Interface main features 
* Show the device info. 
* Manage multiple devices by the group-level. 
* Get lots of raw and aggregated device statistics. 
* Get the health status of the device components. 
* Get and change the device settings. 
* Update the device firmware. 
* Set some automatic action when some condition is met. 

## Help info
Show the XPU Manager CLI help info. 
```
./xpumcli 
Intel XPU Manager Command Line Interface -- v1.0 
Intel XPU Manager Command Line Interface provides the Intel datacenter GPU model and monitoring capabilities. It can also be used to change the Intel datacenter GPU settings and update the firmware.  
Intel XPU Manager is based on Intel oneAPI Level Zero. Before using Intel XPU Manager, the GPU driver and Intel oneAPI Level Zero should be installed rightly.  
 
Supported devices: 
  - Intel ATS-M1/ATS-M3/ATS-P 
 
Usage: xpumcli [Options]
  xpumcli -v
  xpumcli -h
  xpumcli discovery

Optional arguments:
  -h, --help                  Print this help message and exit.
  -v, --version               Display version information and exit.
  
  discovery                   Discover the GPU devices installed on this machine and provide the device info.
  group                       Group the managed GPU devices.
  agentset                    Get or change some XPU Manager settings. 
  stats                       List the GPU device aggregated statistics that are collected by XPU Manager.
  health                      Get the GPU device component health status.
  diag                        Run some test suites to diagnose GPU.
  updatefw                    Update GPU firmware.
  config                      Get and change the GPU settings.
  dump                        Dump device statistics data.
  topology                    Get the GPU to CPU and GPU to PCIe switch topology info.
  policy                      Get and set the GPU policies.
```
  
Show Intel XPU Manager version and Level Zero version. 
```
./xpumcli -v
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
./xpumcli discovery -h

Discover the GPU devices installed on this machine and provide the device info. 

Usage: xpumcli discovery [Options]
  xpumcli discovery
  xpumcli discovery -d [deviceId]
  xpumcli discovery -d [deviceId] -j

Options:
  -h,--help                   Print this help message and exit.
  -j,--json                   Print result in JSON format

  -d,--device                 Device ID to query. It will show more detailed info.

```



Discover the devices in this machine
```
./xpumcli discovery
+-----------+--------------------------------------------------------------------------------------+
| Device ID | Device Information                                                                   |
+-----------+--------------------------------------------------------------------------------------+
| 0         | Device Name: Intel(R) Graphics [0x020a]                                              |
|           | Vendor Name: Intel(R) Corporation                                                    |
|           | UUID: 00000000-0000-0000-0000-020a00008086                                           |
|           | PCI BDF Address: 0000:4d:00.0                                                        |
+-----------+--------------------------------------------------------------------------------------+
```

Discover the devices in this machine and get the JSON format output
```
./xpumcli discovery -j
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
./xpumcli discovery -d 0
+-----------+--------------------------------------------------------------------------------------+
| Device ID | Device Information                                                                   |
+-----------+--------------------------------------------------------------------------------------+
| 0         | Device Type: GPU                                                                     |
|           | Device Name: Intel(R) Graphics [0x020a]                                              |
|           | Vendor Name: Intel(R) Corporation                                                    |
|           | UUID: 00000000-0000-0000-0000-020a00008086                                           |
|           | Serial Number: unknown                                                               |
|           | Core Clock Rate: 1400 MHz                                                            |
|           |                                                                                      |
|           | Driver Version: 16929133                                                             |
|           | Firmware Name: GSC                                                                   |
|           | Firmware Version: ATS0_1.1                                                           |
|           | Firmware Name: AMC                                                                   |
|           | Firmware Version: 3.4.0.0                                                            |
|           |                                                                                      |
|           | PCI BDF Address: 0000:4d:00.0                                                        |
|           | PCI Slot: Riser 1, slot 1                                                            |
|           | PCIe Generation: 4                                                                   |
|           | PCIe Max Link Width: 16                                                              |
|           |                                                                                      |
|           | Memory Physical Size: 32768.00 MiB                                                   |
|           | Max Mem Alloc Size: 4095.99 MiB                                                      |
|           | Number of Memory Channels: 2                                                         |
|           | Memory Bus Width: 128                                                                |
|           | Max Hardware Contexts: 65536                                                         |
|           | Max Command Queue Priority: 0                                                        |
|           |                                                                                      |
|           | Number of Tiles: 2                                                                   |
|           | Number of Slices: 2                                                                  |
|           | Number of Sub Slices per Slice: 30                                                   |
|           | Number of EUs per Sub Slice: 16                                                      |
|           | Number of Threads per EU: 8                                                          |
|           | Physical EU SIMD Width: 8                                                            |
+-----------+--------------------------------------------------------------------------------------+
```

## Manage the devices in groups
Help info of the group operation
```
./xpumcli group -h

Group the managed GPU devices.

Usage: xpumcli group [Options]
  xpumcli group -c -n [groupName]
  xpumcli group -a -g [groupId] -d [deviceIds]
  xpumcli group -r -d [deviceIds] -g [groupId]
  xpumcli group -D -g [groupId]
  xpumcli -l
  xpumcli -l -j
  xpumcli -l -g [groupId]


Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -c,--create                 Create a group.
  -D,--delete                 Delete a group.
  -l,--list                   List the groups info.
  -a,--add                    Add devices to a group.
  -r,--remove                 Remove devices from group.
  -g,--group                  Group ID.
  -n,--name                   Group name.
  -d,--device                 Device IDs.
```
 
Create a group                        
```
./xpumcli group -c -n "testgroup"
+----------+---------------------------------------------------------------------------------------+
| Group ID | Group Properties                                                                      |
+----------+---------------------------------------------------------------------------------------+
| 1        | Group Name: testgroup                                                                 |
|          | Device IDs: []                                                                        |
+----------+---------------------------------------------------------------------------------------+
```
 
Add device to a group
```
./xpumcli group -a -g 1 -d 0
Successfully add device [0] to group 1
+----------+---------------------------------------------------------------------------------------+
| Group ID | Group Properties                                                                      |
+----------+---------------------------------------------------------------------------------------+
| 1        | Group Name: testgroup                                                                 |
|          | Device IDs: [0]                                                                       |
+----------+---------------------------------------------------------------------------------------+
```
 
List a group info 
```
./xpumcli group -l
+----------+---------------------------------------------------------------------------------------+
| Group ID | Group Properties                                                                      |
+----------+---------------------------------------------------------------------------------------+
| 1        | Group Name: testgroup                                                                 |
|          | Device IDs: [0]                                                                       |
+----------+---------------------------------------------------------------------------------------+
```
 
Remove devices from a group
```
./xpumcli group -r -d 0 -g 1
Successfully remove device [0] from group 1
+----------+---------------------------------------------------------------------------------------+
| Group ID | Group Properties                                                                      |
+----------+---------------------------------------------------------------------------------------+
| 1        | Group Name: testgroup                                                                 |
|          | Device IDs: []                                                                        |
+----------+---------------------------------------------------------------------------------------+
```
 
Remove a group 
```
./xpumcli group -D -g 1
Successfully remove the group
```
 
## Get and change the Intel XPU Manager settings
Help message of the "agentset" subcommand.
```
./xpumcli agentset
Get or change some XPU Manager settings. 

usage: xpumcli agentset [Options]
  xpumcli agentset -l
  xpumcli agentset -l -j
  xpumcli agentset -t 200


optional arguments:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -l,--list                   Display all agent settings
  -t,--time                   Set the time interval (in milliseconds) by which XPU Manager daemon retrieve raw gpu statistics. Valid values include 100,200,500,1000.
```
 
List the XPU Manager settings
```
./xpumcli agentset -l
+------------------------+-------------------------------------------------------------------------+
| Name                   | Value                                                                   |
+------------------------+-------------------------------------------------------------------------+
| Sampling Interval (ms) | 1000                                                                    |
+------------------------+-------------------------------------------------------------------------+
```

Change the XPU Manager sampling period
```
./xpumcli agentset -t 200
+------------------------+-------------------------------------------------------------------------+
| Name                   | Value                                                                   |
+------------------------+-------------------------------------------------------------------------+
| Sampling Interval (ms) | 200                                                                     |
+------------------------+-------------------------------------------------------------------------+
```


## Get the aggregated device statistics
Help info for getting the GPU device aggregated statistics 
```
./xpumcli stats -h

List the GPU aggregated statistics since last execution of this command or XPU Manager daemon is started.

Usage: xpumcli stats [Options]
  xpumcli stats
  xpumcli stats -d [deviceId]
  xpumcli stats -g [groupId]

Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 The device ID to query
  -g,--group                  The group ID to query
```
 
List the GPU device aggregated statistics that are collected by XPU Manager
```
./xpumcli stats -d 0
+------------------------------+-------------------------------------------------------------------+
| Device ID                    | 0                                                                 |
+------------------------------+-------------------------------------------------------------------+
| Start Time                   | 2021-11-02 14:44:25.000                                           |
| End Time                     | 2021-11-02 14:47:49.021                                           |
| Elapsed Time (Second)        | 204                                                               |
| Energy Consumed (J)          | Tile 0: 18264.05, Tile 1: 18264.06                                |
| GPU Utilization (%)          | Tile 0: 0, Tile 1: 0                                              |
| EU Array Active (%)          | Tile 0: 0, Tile 1: 0                                              |
| EU Array Stall (%)           | Tile 0: 0, Tile 1: 0                                              |
| EU Array Idle (%)            | Tile 0: 0, Tile 1: 0                                              |
+------------------------------+-------------------------------------------------------------------+
| Reset                        | 0, total: 0                                                       |
| Programming Errors           | Tile 0: 0, total: 0; Tile 1: 0, total: 0                          |
| Driver Errors                | Tile 0: 0, total: 0; Tile 1: 0, total: 0                          |
| Cache Errors Correctable     | Tile 0: 0, total: 0; Tile 1: 0, total: 0                          |
| Cache Errors Uncorrectable   | Tile 0: 0, total: 0; Tile 1: 0, total: 0                          |
+------------------------------+-------------------------------------------------------------------+
| GPU Power (W)                | Tile 0: avg: 88, min: 88, max: 90， current: 89                   |
|                              | Tile 1: avg: 88, min: 88, max: 90， current: 89                   |
+------------------------------+-------------------------------------------------------------------+
| GPU Frequency (MHz)          | Tile 0: avg: 1400, min: 1400, max: 1400, current: 1400            |
|                              | Tile 1: avg: 300, min: 300, max: 300, current: 300                |
+------------------------------+-------------------------------------------------------------------+
| GPU Core Temperature         | Tile 0: avg: 36, min: 36, max: 36, current: 36                    |
| (Celsius Degree)             | Tile 1: avg: 33, min: 33, max: 34, current: 33                    |
+------------------------------+-------------------------------------------------------------------+
| GPU Memory Temperature       | Tile 0: avg: 36, min: 36, max: 36, current: 36                    |
| (Celsius Degree)             | Tile 1: avg: 33, min: 33, max: 34, current: 33                    |
+------------------------------+-------------------------------------------------------------------+
| GPU Memory Read (kB/s)       | Tile 0: avg: 100, min: 90, max: 240, current: 100                 |
|                              | Tile 1: avg: 100, min: 90, max: 240, current: 100                 |
+------------------------------+-------------------------------------------------------------------+
| GPU Memory Write (kB/s)      | Tile 0: avg: 100, min: 90, max: 240, current: 100                 |
|                              | Tile 1: avg: 100, min: 90, max: 240, current: 100                 |
+------------------------------+-------------------------------------------------------------------+
| GPU Memory Bandwidth (%)     | Tile 0: avg: 10, min: 9, max: 15, current: 10                     |
|                              | Tile 1: avg: 10, min: 9, max: 15, current: 10                     |
+------------------------------+-------------------------------------------------------------------+
| GPU Memory Used (MiB)        | Tile 0: avg: 500, min: 100, max: 700, current: 400                |
|                              | Tile 1: avg: 500, min: 100, max: 700, current: 400                |
+------------------------------+-------------------------------------------------------------------+
```
 
## Get the device health status
Help info of get GPU device component health status
```
./xpumcli health

Get the GPU device component health status

Usage: xpumcli health [Options]
  xpumcli health -l
  xpumcli health -d [deviceId] -c [componentTypeId] --threshold  [threshold]
  xpumcli health -d [deviceId] -c [componentTypeId] --threshold [threshold] -j
  xpumcli health -g [groupId] -c [componentTypeId] --threshold [threshold]

optional arguments:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -l,--list                   Display health info for all devices
  -d,--device                 The device ID
  -g,--group                  The group ID
  -c,--component              Component types
                                1. GPU Core Temperature
                                2. GPU Memory Temperature
                                3. GPU Power
                                4. GPU Memory
  --threshold                 Set custom threshold for device component
```
 
Get the GPU device component health status. There are some build-in thresholds for the GPU telemetries. You may also set your custom threshold to help monitor the GPU component health status. 
```
./xpumcli health -l
+------------------------------+-------------------------------------------------------------------+
| Device ID                    | 0                                                                 |
+------------------------------+-------------------------------------------------------------------+
| 1. GPU Core Temperature      | Status: Ok                                                        |
|                              | Description: All temperature sensors are healthy.                 |
|                              | Throttle Threshold: 105 Celsius Degree                            |
|                              | Shutdown Threshold: 130 Celsius Degree                            |
|                              | Custom Threshold: -1                                              |
+------------------------------+-------------------------------------------------------------------+
| 2. GPU Memory Temperature    | Status: Ok                                                        |
|                              | Description: All temperature sensors are healthy.                 |
|                              | Throttle Threshold: 85  Celsius Degree                            |
|                              | Shutdown Threshold: 100 Celsius Degree                            |
|                              | Custom Threshold: -1                                              |
+------------------------------+-------------------------------------------------------------------+
| 3. GPU Power                 | Status: Ok                                                        |
|                              | Description: All power domains are healthy.                       |
|                              | Throttle Threshold: 150 watts                                     |
|                              | Custom Threshold: -1                                              |
+------------------------------+-------------------------------------------------------------------+
| 4. GPU Memory                | Status: Ok                                                        |
|                              | Description: All memory channels are healthy.                     |
+------------------------------+-------------------------------------------------------------------+
```
 
Change the component custom temperature threshold 
```
./xpumcli health -d 0 -c 1 --threshold 90
+------------------------------+-------------------------------------------------------------------+
| Device ID                    | 0                                                                 |
+------------------------------+-------------------------------------------------------------------+
| 1. GPU Core Temperature      | Status: Ok                                                        |
|                              | Description: All temperature sensors are healthy.                 |
|                              | Throttle Threshold: 105 Celsius Degree                            |
|                              | Shutdown Threshold: 130 Celsius Degree                            |
|                              | Custom Threshold: 90 Celsius Degree                               |
+------------------------------+-------------------------------------------------------------------+
```
 
# Dump the device statistics in CSV format
Help info of the device statistics dump.
```
./xpumcli dump

Dump device statistics data

Usage: xpumcli dump [Options]
  xpumcli dump -d [deviceId] -t [deviceTileId] -m [metricsIds] -i [timeInterval] -n [dumpTimes]
  
  xpumcli dump --rawdata --start -d [deviceId] -t [deviceTileId] -m [metricsIds] 
  xpumcli dump --rawdata --list
  xpumcli dump --rawdata --stop [taskId]

optional arguments:
  -h,--help                   Print this help message and exit

  -d,--device                 The device id to query
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
                                12. Reset Counter, per GPU.
                                13. Programming Errors, per tile.
                                14. Driver Errors, per tile.
                                15. Cache Errors Correctable, per tile.
                                16. Cache Errors Uncorrectable, per tile. 
                                17. GPU Memory Bandwidth Utilization. (%)
                                18. GPU Memory Used (MiB)
  
  -i                          The interval (in seconds) to dump the device statistics to screen. The interval will be XPU Manager sampling period if this parameter is not specified. 
  -n                          Number of the device statistics dump to screen. The dump will never be ended if this parameter is not specified. 
  
  --rawdata                   Dump the required raw statistics to a file in background. 
  --start                     Start a new background task to dump the raw statistics to a file. The task ID and the generated file path are returned.
  --stop                      Stop one active dump task.
  --list                      List all the active dump tasks. 
```

Dump the device statistics to screen in CSV format.
```
./xpumcli dump -d 0 -t 0 -m 0,1,2 -i 1 -n 5
Timestamp,DeviceId,TileId,GPU Utilization (%),GPU Power (W),GPU Frequency (MHz)
2021-11-08 13:31:43.100, 00, 0, 000,    , 0300
2021-11-08 13:31:44.100, 00, 0, 000,    , 0300
2021-11-08 13:31:45.100, 00, 0, 046,    , 1100
2021-11-08 13:31:46.100, 00, 0, 000,    , 0300
2021-11-08 13:31:47.100, 00, 0, 000,    , 0300
```

Start to dump the device raw statistics to the CSV file.
```
xpumcli dump --rawdata --start -d 0 -t 0 -m 0,1,2 
Task 0 is started.
Dump file path: /opt/xpum/dump/dump-output-e4439267203fb5277d347e6cd6e440b5.csv
```

List all the active dump tasks.
```
xpumcli dump --rawdata --list
Task 0 is running. 
Task 1 is running.
```

Stop the dump task. 
```
xpumcli dump --rawdata --stop 0
Task 0 is stopped. 
Dump file path: /opt/xpum/dump/dump-output-e4439267203fb5277d347e6cd6e440b5.csv
```

## Get the device topology
Help info of get GPU to CPU and GPU to PCIe switch topology
```
./xpumcli topology

Get the GPU to CPU and GPU to PCIe switch topology info

Usage: xpumcli topology [Options]
  xpumcli topology -d [deviceId]
  xpumcli topology -d [deviceId] -j

optional arguments:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 The device ID to query
```

Get the info of the CPUs and PCIe switches which are connected to the GPU
```
./xpumcli topology -d 0
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
  
  
## Update the GPU firmware
Help info of updating GPU firmware
```
./xpumcli updatefw

Update GPU firmware

Usage: xpumcli updatefw [Options]
  xpumcli updatefw -d [deviceId] -t [firmwareName] -f [imageFilePath]

Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 The device ID
  -t,--type                   The firmware name. Valid options: GSC, AMC. AMC firmware update just works for ATS-P card so far.
  -f,--file                   The firmware image file path on this server.
```

Update GPU firmware
```
./xpumcli updatefw -d 0 -t GSC -f /home/test/tools/ATS.PS.B.P.Si.2021.WW41.5_25MHz_Quad_DAMen_IFWI.bin
Start to update firmware:
Firmware name: GSC
Image path: /home/test/tools/ATS.PS.B.P.Si.2021.WW41.5_25MHz_Quad_DAMen_IFWI.bin
Update firmware successfully. 
```
 

## Get and change the GPU settings
Help info of getting/changing the GPU settings
```
./xpumcli config

Get and change the GPU settings.

Usage: xpumcli config [Options]
  xpumcli config -d [deviceId]
  xpumcli config -d [deviceId] -t [tileId] --frequencyrange [minFrequency,maxFrequency]
  xpumcli config -d [deviceId] -t [tileId] --powerlimit [powerValue,averageWindow]
  xpumcli config -d [deviceId] -t [tileId] --standby [standbyMode]
  xpumcli config -d [deviceId] -t [tileId] --scheduler [schedulerMode]
  xpumcli config -d [deviceId] --reset
  


Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 The device ID
  -t,--tile                   The tile ID

  --frequencyrange            GPU tile-level core frequency range.
  --powerlimit                Tile-level power limit. 
  --standby                   Tile-level standby mode. Valid options: "default"; "never".
  --scheduler                 Tile-level scheduler mode. Value options: "timeout",timeoutValue (us); "timeslice",interval (us),yieldtimeout (us);
                                "exclusive".
  --reset                     Hard reset the GPU. All applications that are currently using this device will be forcibly killed. 
```

show the GPU settings
```
./xpumcli config -d 0
+-------------+-------------------+----------------------------------------------------------------+
| Device Type | Device ID/Tile ID | Configuration                                                  |
+-------------+-------------------+----------------------------------------------------------------+
| GPU         | 0/0               | Power Limit (w): 300.0                                         |
|             |                   |   Valid Range: 0 to 500                                        |
|             |                   | Power Average Window (ms): 1                                   |
|             |                   |   Valid Range: 1 to 60000                                      |
|             |                   |                                                                |
|             |                   | GPU Min Frequency(MHz): 300.0                                  |
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
+-------------+-------------------+----------------------------------------------------------------+
| GPU         | 0/1               | Power Limit (w): 300.0                                         |
|             |                   |   Valid Range: 0 to 500                                        |
|             |                   | Power Average Window (ms): 1                                   |
|             |                   |   Valid Range: 1 to 60000                                      |
|             |                   |                                                                |
|             |                   | GPU Min Frequency(MHz): 300.0                                  |
|             |                   | GPU Max Frequency(MHz): 1300.0                                 |
|             |                   |   Valid Options: 300, 350, 400, 450, 500,550, 600, 650, 700    |
|             |                   |     750, 800, 850, 900, 950, 1000, 1050, 1100, 1150, 1200      |
|             |                   |     1250, 1300                                                 |
|             |                   |                                                                |
|             |                   | Standby Mode: default                                          |
|             |                   |                                                                |
|             |                   | Scheduler Mode: timeslice                                      |
|             |                   |   Interval(us): 5000                                           |
|             |                   |   Yield Timeout (us): 640000                                   |
+-------------+-------------------+----------------------------------------------------------------+
```
 
Change the GPU tile core frequency range.
```
xpumcli config -d 0 -t 0 --frequencyrange 1200,1300
Succeed to change the core frequency range on GPU 0 tile 0.
```
 
Change the GPU power limit.
```
./xpumcli config -d 0 -t 0 --powerlimit 299,1000
Succeed to set the power limit on GPU 0 tile 0.
```
 
Change the GPU tile standby mode.
```
./xpumcli config -d 0 -t 0 --standby never
Succeed to change the standby mode on GPU 0.
```

Change the GPU tile scheduler mode.
```
./xpumcli config -d 0 -t 0 --scheduler timeout,640000
Succeed to change the scheduler mode on GPU 0 tile 0.
```

Reset the GPU.
```
./xpumcli config -d 0 --reset
The process（es） below are using this device. 
  PID: 633972, Command: ./ze_gemm
  PID: 633973, Command: ./ze_gemm

All process(es) above will be forcibly killed if you reset it. Do you want to continue? (Y/N) Y
Succeed to reset the GPU 0.
```

## Get and set the policy, automatic action triggered by the condition
The supported policies are list in the table below. For example, if the "GPU Core Temperature" policy is set and one GPU tile temperature is more than the specified threshold, the GPU throttling action will be taken automatically. For "Cache Errors Uncorrectable", if it happens, a resetting GPU action will be taken. The temperature and errors are all based on tile-level.  

| Types                         | Conditions     | Actions                 |  
|:------------------------------|:---------------|:------------------------|
| 1. GPU Core Temperature       | 1. More than   | 1. Throttle GPU Core    |  
| 2. Programming Errors         | 1. More than   | 2. Reset GPU            |  
| 3. Driver Errors              | 1. More than   | 2. Reset GPU            |  
| 4. Cache Errors Correctable   | 1. More than   | 2. Reset GPU            |  
| 5. Cache Errors Uncorrectable | 2. When occur  | 2. Reset GPU            |  


Help info for GPU policy
```
./xpumcli policy

Get and set the GPU policis.

Usage: xpumcli policy [Options]
  xpumcli policy -d [deviceId] -l
  xpumcli policy -d [deviceId] -l -j
  xpumcli policy -g [groupId] -l
  xpumcli policy -g [groupId] -l -j
  xpumcli policy -c -d [deviceId] --type [policyTypeValue] --condition 1 --threshold [threshold]  --action [policyActionValue]
  xpumcli policy -c -d [deviceId] --type [policyTypeValue] --condition 2 --action [policyActionValue]
  xpumcli policy -c -g [groupId] --type 1 --threshold [threshold]  --action 1 --throttlefrequencymin [frequencyMinValue] --throttlefrequencymax [frequencyMaxValue]
  xpumcli policy -r -d [deviceId] --type [policyTypeValue]
  xpumcli policy -r -g [groupId] --type [policyTypeValue]
  


Options:
  -h,--help                   Print this help message and exit.
  -j,--json                   Print result in JSON format.

  -d,--device                 The device ID.
  -g,--group                  The group ID.

  -l,--list                   List all policies.
  --listalltypes              List all policy types, including the supported condition and action.
  -c,--create                 Create one policy
  -r,--remove                 Remove one policy. Only the policy is removed and the changed GPU settings will not be resumed. 
  
  --type                      Policy types.
                                1. GPU Core Temperature
                                2. Programming Errors
                                3. Driver Errors
                                4. Cache Errors Correctable
                                5. Cache Errors Uncorrectable
  --condition                 Conditions.
                                1. More than
                                2. When occur
  --threshold                 Threshold
  --action                    Policy action.
                                1. Throttle GPU Core Frequency
                                2. Reset GPU
  --throttlefrequencymin      Throttle GPU frequency to min value
  --throttlefrequencymax      Throttle GPU frequency to max value
```
  
Create a policy to throttle GPU when the GPU tile temperature is a little high. 
```
./xpumcli policy -c -d 0 --type 1 --condition 1 --threshold 95  --action 1 --throttlefrequencymin 300 --throttlefrequencymax 400
Succeed to set the "1. GPU Core Temperature" policy.
```

List all supported policies
```
./xpumcli policy --listalltypes
+-------------------------------+------------------+-----------------------------------------------+
| Types                         |Conditions        | Actions                                       |
+-------------------------------+------------------+-----------------------------------------------+
| 1. GPU Core Temperature       | 1. More than     | 1. Throttle GPU Core Frequency                |
| 2. Programming Errors         | 1. More than     | 2. Reset GPU                                  |
| 3. Driver Errors              | 1. More than     | 2. Reset GPU                                  |
| 4. Cache Errors Correctable   | 1. More than     | 2. Reset GPU                                  |
| 5. Cache Errors Uncorrectable | 2. When occur    | 2. Reset GPU                                  |
+-------------------------------+------------------+-----------------------------------------------+
```

List all policies set on the GPU. 
```
./xpumcli policy -l -d 0
+-----------+-------------------------------+------------------+-----------------------------------+
| Device ID | Types                         | Conditions       | Actions                           |
+-----------+-------------------------------+------------------+-----------------------------------+
| 0         | 1. GPU Core Temperature       | 1. More than 95  | 1. Throttle GPU Core Frequency    |
|           |                               |                  |      min: 300, max: 400           |
+-----------+-------------------------------+------------------+-----------------------------------+
| 0         | 5. Cache Errors Uncorrectable | 2. When occur    | 2. Reset GPU                      |
+-----------+-------------------------------+------------------+-----------------------------------+
```
  
Remove a policy.
```
./xpumcli policy -r -d 0 --type 1
Succeed to remove the "1. GPU Core Temperature" policy.
```
  
## Diagnose GPU with different test suites
When running tests on GPU, GPU will be used exclusively. There will be obviously performance impact on the GPU. Some CPU performance may also be impacted. 

Help info for GPU diagnostics
```
./xpumcli diag

Run some test suites to diagnose GPU.

Usage: xpumcli diag [Options]
  xpumcli diag -d [deviceId] -l [level]
  xpumcli diag -d [deviceId] -l [level] -j
  xpumcli diag -g [groupId] -l
  xpumcli diag -g [groupId] -l -j
  
Options:
  -h,--help                   Print this help message and exit.
  -j,--json                   Print result in JSON format.

  -d,--device                 The device ID.
  -g,--group                  The group ID.
  -l,--level                  The diagnostics level to run. The valid options include
                                1. quick test
                                2. medium test
                                3. long test
```

Run test to diagnose GPU
```
./xpumcli diag -d 0 -l 1
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







