
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
Intel XPU Manager Command Line Interface provides the Intel data center GPU model and monitoring capabilities. It can also be used to change the Intel data center GPU settings and update the firmware.  
Intel XPU Manager is based on Intel oneAPI Level Zero. Before using Intel XPU Manager, the GPU driver and Intel oneAPI Level Zero should be installed rightly.  
 
Supported devices: 
  - Intel Data Center GPU 
 
Usage: xpumcli [Options]
  xpumcli -v
  xpumcli -h
  xpumcli discovery

Options:
  -h, --help                  Print this help message and exit.
  -v, --version               Display version information and exit.

Subcommands:
  discovery                   Discover the GPU devices installed on this machine and provide the device info.
  group                       Group the managed GPU devices.
  agentset                    Get or change some XPU Manager settings. 
  stats                       List the GPU aggregated statistics since last execution of this command or XPU Manager daemon is started.
  health                      Get the GPU device component health status.
  diag                        Run some test suites to diagnose GPU.
  updatefw                    Update GPU firmware.
  config                      Get and change the GPU settings.
  dump                        Dump device statistics data.
  topology                    get the system topology
  policy                      Get and set the GPU policies.
  amcsensor                   List the AMC real-time sensor reading. 
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
  xpumcli discovery --listamcversions

Options:
  -h,--help                   Print this help message and exit.
  -j,--json                   Print result in JSON format

  -d,--device                 Device ID to query. It will show more detailed info.
  --listamcversions           Show all AMC firmware versions.
  -u,--username               Username used to get AMC version with Redfish Host interface
  -p,--password               Password used to get AMC version with Redfish Host interface

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
|           | Device Name: Intel(R) Graphics [0x56c0]                                              |
|           | Vendor Name: Intel(R) Corporation                                                    |
|           | UUID: 00000000-0000-0000-0000-020a00008086                                           |
|           | Serial Number: unknown                                                               |
|           | Core Clock Rate: 2050 MHz                                                            |
|           | Stepping: C0                                                                         |
|           |                                                                                      |
|           | Driver Version:                                                                      |
|           | Kernel Version: 5.10.54+prerelease35                                                 |
|           | GFX Firmware Name: GFX                                                               |
|           | GFX Firmware Version: DG02_1.3172                                                    |
|           | GFX Data Firmware Name: GFX_DATA                                                     |
|           | GFX Data Firmware Version: 0x141                                                     |
|           |                                                                                      |
|           | PCI BDF Address: 0000:4d:00.0                                                        |
|           | PCI Slot: Riser 1, slot 1                                                            |
|           | PCIe Generation: 4                                                                   |
|           | PCIe Max Link Width: 16                                                              |
|           |                                                                                      |
|           | Memory Physical Size: 14248.00 MiB                                                   |
|           | Max Mem Alloc Size: 4095.99 MiB                                                      |
|           | Number of Memory Channels: 2                                                         |
|           | Memory Bus Width: 128                                                                |
|           | Max Hardware Contexts: 65536                                                         |
|           | Max Command Queue Priority: 0                                                        |
|           |                                                                                      |
|           | Number of EUs: 512                                                                   |
|           | Number of Tiles: 1                                                                   |
|           | Number of Slices: 1                                                                  |
|           | Number of Sub Slices per Slice: 32                                                   |
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

List the versions of all AMC which can be found on this server. 
```
./xpumcli discovery --listamcversions
1 AMC are found
AMC 1 firmware version: 4.0.0.0
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
  xpumcli group -l
  xpumcli group -l -g [groupId]


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
 
Add devices to a group
```
./xpumcli group -a -g 1 -d 0 1
+----------+---------------------------------------------------------------------------------------+
| Group ID | Group Properties                                                                      |
+----------+---------------------------------------------------------------------------------------+
| 1        | Group Name: testgroup                                                                 |
|          | Device IDs: [0,1]                                                                     |
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
| Compute Engines Util(%)      | Tile 0: 0, Tile 1: 0                                              |
| Render Engines Util(%)       | Tile 0: 0, Tile 1: 0                                              |
| Media Engines Util (%)       | Tile 0: 0, Tile 1: 0                                              |
| Copy Engines Util(%)         | Tile 0: 0, Tile 1: 0                                              |
| EU Array Active (%)          | Tile 0: 0, Tile 1: 0                                              |
| EU Array Stall (%)           | Tile 0: 0, Tile 1: 0                                              |
| EU Array Idle (%)            | Tile 0: 0, Tile 1: 0                                              |
+------------------------------+-------------------------------------------------------------------+
| Reset                        | Tile 0: 0, total: 0; Tile 1: 0, total: 0                          |
| Programming Errors           | Tile 0: 0, total: 0; Tile 1: 0, total: 0                          |
| Driver Errors                | Tile 0: 0, total: 0; Tile 1: 0, total: 0                          |
| Cache Errors Correctable     | Tile 0: 0, total: 0; Tile 1: 0, total: 0                          |
| Cache Errors Uncorrectable   | Tile 0: 0, total: 0; Tile 1: 0, total: 0                          |
| Mem Errors Correctable       | Tile 0: 0, total: 0; Tile 1: 0, total: 0                          |
| Mem Errors Uncorrectable     | Tile 0: 0, total: 0; Tile 1: 0, total: 0                          |
+------------------------------+-------------------------------------------------------------------+
| GPU Power (W)                | Tile 0: avg: 88, min: 88, max: 90, current: 89                    |
|                              | Tile 1: avg: 88, min: 88, max: 90, current: 89                    |
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
| Compute Engine Util (%)      | Tile 0:                                                           |
|                              |   Engine 0: 0, Engine 1: 100, Engine 2: 0, Engine 3: 50           |
|                              |   Engine 4: 0, Engne 5: 100, Engine 6: 0, Engine 7: 50            |
|                              | Tile 1:                                                           |
|                              |   Engine 0: 0, Engine 1: 100, Engine 2: 0, Engine 3: 50           |
|                              |   Engine 4: 0, Engine 5: 100, Engine 6: 0, Engine 7: 50           |
+------------------------------+-------------------------------------------------------------------+
| Render Engine Util (%)       | Tile 0:                                                           |
|                              |   Engine 0: 0, Engine 1: 100, Engine 2: 0, Engine 3: 50           |
|                              | Tile 1:                                                           |
|                              |   Engine 0: 0, Engine 1: 100, Engine 2: 0, Engine 3: 50           |
+------------------------------+-------------------------------------------------------------------+
| Decoder Engine Util (%)      | Tile 0:                                                           |
|                              |   Engine 0: 0, Engine 1: 100, Engine 2: 0, Engine 3: 50           |
|                              | Tile 1:                                                           |
|                              |   Engine 0: 0, Engine 1: 100, Engine 2: 0, Engine 3: 50           |
+------------------------------+-------------------------------------------------------------------+
| Encoder Engine Util (%)      | Tile 0:                                                           |
|                              |   Engine 0: 0, Engine 1: 100, Engine 2: 0, Engine 3: 50           |
|                              | Tile 1:                                                           |
|                              |   Engine 0: 0, Engine 1: 100, Engine 2: 0, Engine 3: 50           |
+------------------------------+-------------------------------------------------------------------+
| Copy Engine Util (%)         | Tile 0:                                                           |
|                              |   Engine 0: 0, Engine 1: 100, Engine 2: 0, Engine 3: 50           |
|                              | Tile 1:                                                           |
|                              |   Engine 0: 0, Engine 1: 100, Engine 2: 0, Engine 3: 50           |
+------------------------------+-------------------------------------------------------------------+
| Media EM Engine Util (%)     | Tile 0:                                                           |
|                              |   Engine 0: 0, Engine 1: 100, Engine 2: 0, Engine 3: 50           |
|                              | Tile 1:                                                           |
|                              |   Engine 0: 0, Engine 1: 100, Engine 2: 0, Engine 3: 50           |
+------------------------------+-------------------------------------------------------------------+
| 3D Engine Util (%)           | Tile 0:                                                           |
|                              |   Engine 0: 0, Engine 1: 100, Engine 2: 0, Engine 3: 50           |
|                              | Tile 1:                                                           |
|                              |   Engine 0: 0, Engine 1: 100, Engine 2: 0, Engine 3: 50           |
+------------------------------+-------------------------------------------------------------------+
| Xe Link Throughput (kB/s)    | 0/0 -> 1/1: avg: 500, min: 100, max: 700, current: 400            |
|                              | 0/1 -> 1/0: avg: 500, min: 100, max: 700, current: 400            |
|                              | 1/1 -> 0/0: avg: 500, min: 100, max: 700, current: 400            |
|                              | 1/0 -> 0/1: avg: 500, min: 100, max: 700, current: 400            |
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
                                5. Xe Link Port
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
| 5. Xe Link Port              | Status: Ok                                                        |
|                              | Description: All ports are healthy.                               |
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
 
## Dump the device statistics in CSV format
Help info of the device statistics dump. Please note that the metrics 'GPU Energy Consumed', 'Reset Counter', 'Programming Errors', 'Driver Errors', 'Cache Errors Correctable' and 'Cache Errors Uncorrectable' are not implemented in dump sub-command so far. Please do not dump these metrics. 
```
./xpumcli dump

Dump device statistics data.

Usage: xpumcli dump [Options]
  xpumcli dump -d [deviceIds] -t [deviceTileId] -m [metricsIds] -i [timeInterval] -n [dumpTimes]
  
  xpumcli dump --rawdata --start -d [deviceId] -t [deviceTileId] -m [metricsIds] 
  xpumcli dump --rawdata --list
  xpumcli dump --rawdata --stop [taskId]

optional arguments:
  -h,--help                   Print this help message and exit

  -d,--device                 The device id(s) to query
  -t,--tile                   The device tile ID to query. If the device has only one tile, this parameter should not be specified. 
  -m,--metrics                Metrics type to collect raw data, options. Separated by the comma.
                                0. GPU Utilization (%), GPU active time of the elapsed time, per tile
                                1. GPU Power (W), per GPU and per tile
                                2. GPU Frequency (MHz), per tile
                                3. GPU Core Temperature (Celsius Degree), per tile
                                4. GPU Memory Temperature (Celsius Degree), per tile
                                5. GPU Memory Utilization (%), per tile
                                6. GPU Memory Read (kB/s), per tile
                                7. GPU Memory Write (kB/s), per tile
                                8. GPU Energy Consumed (J), per tile
                                9. GPU EU Array Active (%), the normalized sum of all cycles on all EUs that were spent actively executing instructions. Per tile.
                                10. GPU EU Array Stall (%), the normalized sum of all cycles on all EUs during which the EUs were stalled. Per tile. At least one thread is loaded, but the EU is stalled. Per tile.
                                11. GPU EU Array Idle (%), the normalized sum of all cycles on all cores when no threads were scheduled on a core. Per tile.
                                12. Reset Counter, per tile.
                                13. Programming Errors, per tile.
                                14. Driver Errors, per tile.
                                15. Cache Errors Correctable, per tile.
                                16. Cache Errors Uncorrectable, per tile. 
                                17. GPU Memory Bandwidth Utilization. (%)
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

  
  -i                          The interval (in seconds) to dump the device statistics to screen. Default value: 1 second. 
  -n                          Number of the device statistics dump to screen. The dump will never be ended if this parameter is not specified. 
  
  --rawdata                   Dump the required raw statistics to a file in background. 
  --start                     Start a new background task to dump the raw statistics to a file. The task ID and the generated file path are returned.
  --stop                      Stop one active dump task.
  --list                      List all the active dump tasks. 
```

Dump the device statistics to screen in CSV format.
```
./xpumcli dump -d 0 -t 0 -m 0,1,2,21,22 -i 1 -n 5
Timestamp,DeviceId,TileId,GPU Utilization (%),GPU Power (W),GPU Frequency (MHz),XL 0/0->1/1 (kB/s),XL 1/1->0/0 (kB/s),Compute Engine 0 (%), Compute Engine 1 (%)
13:31:43.100, 00, 0, 000,    , 0300, 400, 700, 100, 0
13:31:44.100, 00, 0, 000,    , 0300, 400, 700, 100, 0
13:31:45.100, 00, 0, 046,    , 1100, 400, 700, 100, 0
13:31:46.100, 00, 0, 000,    , 0300, 400, 700, 100, 0
13:31:47.100, 00, 0, 000,    , 0300, 400, 700, 100, 0
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

## Get the system topology
Help info of get the system topology
```
./xpumcli topology

Get the system topology info.

Usage: xpumcli topology [Options]
  xpumcli topology -d [deviceId]
  xpumcli topology -d [deviceId] -j
  xpumcli topology -f [filename]
  xpumcli topology -m

optional arguments:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 The device ID to query
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
  
Generate the system hardware topology to a XML file. 
```
./xpumcli topology -f topo.xml
The system topology is generated to the file topo.xml. 
```

Generate the CPU/GPU topology matrix. 
```
./xpumcli topology -m

         GPU 0/0|GPU 0/1|GPU 1/0|GPU 1/1|CPU Affinity
GPU 0/0 |S      |MDF    |XL*    |XL16   |0-23,48-71
GPU 0/1 |MDF    |S      |XL16   |XL*    |0-23,48-71
GPU 1/0 |XL*    |XL16   |S      |MDF    |24-47,72-95
GPU 1/1 |XL16   |XL*    |MDF    |S      |24-47,72-95
```
  
  
## Update the GPU firmware
Help info of updating GPU firmware
```
./xpumcli updatefw

Update GPU firmware.

Usage: xpumcli updatefw [Options]
  xpumcli updatefw -d [deviceId] -t GFX -f [imageFilePath]
  xpumcli updatefw -t AMC -f [imageFilePath]

Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 The device ID
  -t,--type                   The firmware name. Valid options: GFX, GFX_DATA, AMC. AMC firmware update just works on Intel M50CYP server (BMC firmware version is 2.82 or newer) and Supermicro SYS-620C-TN12R server (BMC firmware version is 11.01 or newer).
  -f,--file                   The firmware image file path on this server.
  -u,--username               Username used to update AMC firmware through Redfish Host interface
  -p,--password               Password used to update AMC firmware through Redfish Host interface
  -y,--assumeyes              Assume that the answer to any question which would be asked is yes
```

Update GPU GFX firmware
```
./xpumcli updatefw -d 0 -t GFX -f /home/test/tools/ATS_M3.bin
This GPU card has multiple cores. This operation will update all firmware. Do you want to continue? (y/n) y
Start to update firmware:
Firmware name: GFX
Image path: /home/test/tools/ATS_M3.bin
Update firmware successfully. Please reboot OS to take effect. 
```

Update GPU AMC firmware
```
./xpumcli updatefw -t AMC -f /home/dcm/ats_m_amc_v_3_6_3_0.bin
CAUTION: it will update the AMC firmware of all cards and please make sure that you install the GPUs of the same model. Updating AMC firmware may cause OS to reboot.
Please confirm to proceed ( Y/N ) ?
Y
Start to update firmware
Firmware Name: AMC
Image path: /home/dcm/ats_m_amc_v_3_6_3_0.bin
..............Update firmware successfully. Please reboot OS to take effect. 

```
 

## Get and change the GPU settings
Help info of getting/changing the GPU settings
```
./xpumcli config

Get and change the GPU settings.

Usage: xpumcli config [Options]
  xpumcli config -d [deviceId]
  xpumcli config -d [deviceId] -t [tileId] --frequencyrange [minFrequency,maxFrequency]
  xpumcli config -d [deviceId] --powerlimit [powerValue]
  xpumcli config -d [deviceId] --memoryecc [0|1] 0:disable; 1:enable
  xpumcli config -d [deviceId] -t [tileId] --standby [standbyMode]
  xpumcli config -d [deviceId] -t [tileId] --scheduler [schedulerMode]
  xpumcli config -d [deviceId] -t [tileId] --performancefactor [engineType,factorValue]
  xpumcli config -d [deviceId] -t [tileId] --xelinkport [portId,value]
  xpumcli config -d [deviceId] -t [tileId] --xelinkportbeaconing [portId,value]
  
  
Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

  -d,--device                 The device ID
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

```

show the GPU settings
```
./xpumcli config -d 0
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
xpumcli config -d 0 -t 0 --frequencyrange 1200,1300
Succeed to change the core frequency range on GPU 0 tile 0.
```
 
Change the GPU power limit.
```
./xpumcli config -d 0 --powerlimit 299
Succeed to set the power limit on GPU 0.
```

Change the GPU memory ECC mode.
```
./xpumcli config -d 0 --memoryecc 0
Return: Succeed to change the ECC mode to be disabled on GPU 0. Please reset GPU or reboot OS to take effect.
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
  
Set the performance factor
```
./xpumcli config -d 0 -t 0 --performancefactor compute,70
Succeed to change the compute performance factor to 70 on GPU 0 tile 0.
```

Change the Xe Link port status
```
./xpumcli config -d 0 -t 0 --xelinkport 0,1
Succeed to change Xe Link port 0 to up.
```

Change the Xe Link port beaconing status
```
./xpumcli config -d 0 -t 0 --xelinkportbeaconing 0,1
Succeed to change Xe Link port 0 beaconing to on.
```
  
## Get and set the policy, automatic action triggered by the condition
The supported policies are list in the table below. For example, if the "GPU Core Temperature" policy is set and one GPU tile temperature is more than the specified threshold, the GPU throttling action will be taken automatically. 

| Types                         | Conditions     | Actions                 |  
|:------------------------------|:---------------|:------------------------|
| 1. GPU Core Temperature       | 1. More than   | 1. Throttle GPU Core    |


Help info for GPU policy
```
./xpumcli policy

Get and set the GPU policies.

Usage: xpumcli policy [Options]
  xpumcli policy -l
  xpumcli policy --listalltypes 
  xpumcli policy -d [deviceId] -l
  xpumcli policy -d [deviceId] -l -j
  xpumcli policy -g [groupId] -l
  xpumcli policy -g [groupId] -l -j  
  xpumcli policy -c -d [deviceId] --type 1 --condition 1 --threshold [threshold]  --action 1 --throttlefrequencymin [frequencyMinValue] --throttlefrequencymax [frequencyMaxValue]
  xpumcli policy -c -g [groupId] --type 1 --condition 1 --threshold [threshold]  --action 1 --throttlefrequencymin [frequencyMinValue] --throttlefrequencymax [frequencyMaxValue]
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
  --condition                 Conditions.
                                1. More than
  --threshold                 Threshold
  --action                    Policy action.
                                1. Throttle GPU Core Frequency
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
  xpumcli diag --precheck
  xpumcli diag --precheck -j
  
Options:
  -h,--help                   Print this help message and exit.
  -j,--json                   Print result in JSON format.

  -d,--device                 The device ID.
  -g,--group                  The group ID.
  -l,--level                  The diagnostic levels to run. The valid options include
                                1. quick test
                                2. medium test - this diagnostic level will have the significant performance impact on the specified GPUs
                                3. long test - this diagnostic level will have the significant performance impact on the specified GPUs
  --precheck                  Do the precheck on the GPU and GPU driver.
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

Pre-check the GPU and GPU driver status
```
xpumcli diag --precheck
+------------------------+-------------------------------------------------------------------------+
| Component              | Status                                                                  |
+------------------------+-------------------------------------------------------------------------+
| GPU                    | 2 (0x56c1), 1 (0x5bc0)                                                  |
| Driver                 | Pass                                                                    |
| GPU Status             | Pass                                                                    |
| CPU Status             | Pass                                                                    |
+------------------------+-------------------------------------------------------------------------+
```

## Show AMC real-time sensor readings
This command only works for Intel(R) Data Center GPU Flex 140/170 on Intel M50CYP servers. 

Help info for get AMC senor readings
```
xpumcli amcsensor -h
List the AMC real-time sensor readings.

Usage: xpumcli amcsensor [Options]
 xpumcli amcsensor
 xpumcli amcsensor -j

Options:
  -h,--help                   Print this help message and exit
  -j,--json                   Print result in JSON format

```

Get the AMC sensor readings.
```
xpumcli amcsensor
+--------+-----------------------------------------------------------------------------------------+
| AMC ID | Sensors                                                                                 |
+--------+-----------------------------------------------------------------------------------------+
| AMC 0  |                                                                                         |
|        | temp_sensor_0 (degrees C): 41                                                           |
|        | temp_sensor_1 (degrees C): 39                                                           |
|        | VCCGT_GPU2_volt (Volts): 0.093                                                          |
|        | soc_die_temp_1 (degrees C): 46                                                          |
|        | soc_die_temp_2 (degrees C): 45                                                          |
+--------+-----------------------------------------------------------------------------------------+
```


