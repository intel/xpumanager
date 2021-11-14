
# Intel XPU Manager Command Line Interface User Guide
This guide describes how to use Intel XPU Manager Command Line Interface to manage Intel GPU devices. 
  

## Intel XPU Manager Command Line Interface main features 
* Show the device info. 
* Manage multiple devices by the group-level. 
* Get lots of raw and aggregrated device statistics. 
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
 
Supported devcies: 
  - Intel ATS-M1/ATS-M3 
 
Usage: xpumcli [Options]
  xpumcli -v
  xpumcli -h
  xpumcli discovery

Optional arguments:
  -h, --help                  Print this help message and exit.
  -v, --version               Display version information and exit.
  
  discovery                   Discover devices on the system.
  group                       Group management.
  agentset                    XPUM agent settings.
  stats                       Display device statistics.
  health                      Display health status of GPUs.
  diag                        System validation/diagnostic.
  updatefw                    Update device firmware.
  config                      Configure settings for devices.
  dump                        Dump device statistics data.
  topology                    Show CPU/GPU/PCIe switch topology.
  policy                      Manager GPU policies.
```
  
Show Intel XPU Manager version and Level Zero version. 
```
./xpumcli -v
CLI:
    Version: 0.1.0
    Build Id: 10

Service:
    Version: 0.1.0
    Build Id: 10
    Level Zero Version:
```

## Discover the devices in this machine
Help info of the "discovery" subcommand
```
./xpumcli discovery -h

Discover the GPU devices installed on this machine and provide the device info. 

Usage: xpumcli discovery [Options]
  xpumcli discovery
  xpumcli discovery -d [deviceId]

Options:
  -h,--help                   Print this help message and exit.
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

Show the detailed info of one device. The device info includes the model, frequency, driver/firwmare info, PCI info, memory info and tile/execution unit info. 
```
./xpumcli discovery -d 0
+--------------------------------------------------------------------------------------------------+
| Device ID | Device Information                                                                   |
+--------------------------------------------------------------------------------------------------+
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
+--------------------------------------------------------------------------------------------------+
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
  xpumcli -l -g [groupId]


Options:
  -h,--help                   Print this help message and exit
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
| 1        | Group Name: "testgroup"                                                               |
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
| 1        | Group Name: "testgroup"                                                               |
|          | Device IDs: [0]                                                                       |
+----------+---------------------------------------------------------------------------------------+
```
 
List a group info 
```
./xpumcli group -l
+----------+---------------------------------------------------------------------------------------+
| Group ID | Group Properties                                                                      |
+----------+---------------------------------------------------------------------------------------+
| 1        | Group Name: "testgroup"                                                               |
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
| 1        | Group Name: "testgroup"                                                               |
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
  xpumcli agentset -t 200
  xpumcli agentset -m [metricsIds]


optional arguments:
  -h,--help                   Print this help message and exit
  -l,--list                   Display all agent settings
  -t,--time                   Set the time interval (in milliseconds) by which XPU Manager daemon retrieve raw gpu statistics. Valid values include 100,200,500,1000.
```
<!--
  -m,--metrics                Metrics types to collect, options:
                                0. GPU Utilization (%), GPU active time of the elapsed time
                                1. GPU Power (W)
                                2. GPU Frequency (MHz)
                                3. GPU Core Temperature (°C)
                                4. GPU Memory Temperature (°C)
                                5. GPU Memory Utilization (%)
                                6. GPU Memory Read (kB/s)
                                7. GPU Memory Write (kB/s)
                                8. GPU Energy Consumed (J)
                                9. GPU EU Array Active (%), the normalized sum of all cycles on all EUs that were spent actively executing instructions.
                                10. GPU EU Array Stall (%), the normalized sum of all cycles on all EUs during which the EUs were stalled. At least one thread is loaded, but the EU is stalled.
                                11. GPU EU Array Idle (%), the normalized sum of all cycles on all cores when no threads were scheduled on a core.
                                12. Reset Count
                                13. Programming Errors
                                14. Driver Errors
                                15. Cache Erros Correctable
                                16. Cache Errors Uncorrectable
-->
 
List the XPU Manager settings
```
./xpumcli agentset -l
+------------------------+-------------------------------------------------------------------------+
| Name                   | Value                                                                   +
+------------------------+-------------------------------------------------------------------------+
| Sampling Interval (ms) | 1000                                                                    +
+------------------------+-------------------------------------------------------------------------+
```

Change the XPU Manager sampling period
```
./xpumcli agentset -t 200
+------------------------+-------------------------------------------------------------------------+
| Name                   | Value                                                                   +
+------------------------+-------------------------------------------------------------------------+
| Sampling Interval (ms) | 200                                                                     +
+------------------------+-------------------------------------------------------------------------+
```
<!--
Change the XPU Manager sampling metrics typs
```
./xpumcli agentset -m 0,2,3,4,5,6,7,9,10,11
+------------------------+-------------------------------------------------------------------------+
| Name                   | Value                                                                   +
+------------------------+-------------------------------------------------------------------------+
| Collected Metrics      | 0,2,3,4,5,6,7,9,10,11                                                   +
+------------------------+-------------------------------------------------------------------------+
```
-->

## Get the aggregrated device statistics
Help info for getting the GPU device aggregrated statistics 
```
./xpumcli stats -h

List the GPU aggregrated statistics since last execution of this command or XPU Manager daemon is started.

Usage: xpumcli stats [Options]
  xpumcli stats
  xpumcli stats -d [deviceId]
  xpumcli stats -g [groupId]

Options:
  -h,--help                   Print this help message and exit
  -d,--device                 The device ID to query
  -g,--group                  The group ID to query
```
 
List the GPU device aggregrated statistics that are collected by XPU Manager
```
./xpumcli stats -d 0
+------------------------------+-------------------------------------------------------------------+
| Device ID                    | 0                                                                 |
+------------------------------+-------------------------------------------------------------------+
| Start Time                   | 2021-11-02 14:44:25.000                                           |
| End Time                     | 2021-11-02 14:47:49.021                                           |
| Elapsed Time (Second)        | 204                                                               |
| Energy Consumed (J)          | 18264.05                                                          |
| GPU Utilization (%)          | Tile 0: 0, Tile 1: 0                                              |
| EU Array Active (%)          | Tile 0: 0, Tile 1: 0                                              |
| EU Array Stall (%)           | Tile 0: 0, Tile 1: 0                                              |
| EU Array Idle (%)            | Tile 0: 0, Tile 1: 0                                              |
+------------------------------+-------------------------------------------------------------------+
| Reset                        | 0                                                                 |
| Programming Errors           | Tile 0: 0, Tile 1: 0                                              |
| Driver Errors                | Tile 0: 0, Tile 1: 0                                              |
| Cache Errors Correctable     | Tile 0: 0, Tile 1: 0                                              |
| Cache Errors Uncorrectable   | Tile 0: 0, Tile 1: 0                                              |
+------------------------------+-------------------------------------------------------------------+
| GPU Power (W)                | avg: 88, min: 88, max: 90， current: 89                           |
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
```
 
## Get the device health status
Help info of get GPU device component health status
```
./xpumcli health

Get the GPU device component health status

Usage: xpumcli health [Options]
  xpumcli health -l
  xpumcli health -d [deviceId] -t [componentName] [threshold]
  xpumcli health -g [groupId] -t [componentName] [threshold]

optional arguments:
  -h,--help                   Print this help message and exit
  -l,--list                   Display health info for all devices
  -d,--device                 The device ID
  -g,--group                  The group ID
  -t,--threshold              Set custom threshold for device component
```
 
Get the GPU device component health status. There are some build-in thresholds for the GPU telemetries. You may also set your custom threshold to help monitor the GPU component health status. 
```
./xpumcli health -l
+------------------------------+-------------------------------------------------------------------+
| Device ID                    | 0                                                                 |
+------------------------------+-------------------------------------------------------------------+
| GPU Temperature              | Status: Ok                                                        |
|                              | Description: All temperature sensors are healthy.                 |
|                              | Throttle Threshold: 105 Celsius Degree                            |
|                              | Shutdown Threshold: 130 Celsius Degree                            |
|                              | Custom Threshold: -1                                              |
+------------------------------+-------------------------------------------------------------------+
| GPU Memory Temperature       | Status: Ok                                                        |
|                              | Description: All temperature sensors are healthy.                 |
|                              | Throttle Threshold: 85  Celsius Degree                            |
|                              | Shutdown Threshold: 100 Celsius Degree                            |
|                              | Custom Threshold: -1                                              |
+------------------------------+-------------------------------------------------------------------+
| Power                        | Status: Ok                                                        |
|                              | Description: All power domains are healthy.                       |
|                              | Throttle Threshold: 150 watts                                     |
|                              | Custom Threshold: -1                                              |
+------------------------------+-------------------------------------------------------------------+
| GPU Memory                   | Status: Ok                                                        |
|                              | Description: All memory channels are healthy.                     |
+------------------------------+-------------------------------------------------------------------+
```
 
Change the component custom temperature threshold 
```
./xpumcli health -d 0 -t "GPU Temperature" 90
+------------------------------+-------------------------------------------------------------------+
| Device ID                    | 0                                                                 |
+------------------------------+-------------------------------------------------------------------+
| GPU Core Temperature         | Status: Ok                                                        |
|                              | Description: All temperature sensors are healthy.                 |
|                              | Throttle Threshold: 105 Celsius Degree                            |
|                              | Shutdown Threshold: 130 Celsius Degree                            |
|                              | Custom Threshold: 90 Celsius Degree                               |
+------------------------------+-------------------------------------------------------------------+
| GPU Memory Temperature       | Status: Ok                                                        |
|                              | Description: All temperature sensors are healthy.                 |
|                              | Throttle Threshold: 85  Celsius Degree                            |
|                              | Shutdown Threshold: 100 Celsius Degree                            |
|                              | Custom Threshold: -1                                              |
+------------------------------+-------------------------------------------------------------------+
| Power                        | Status: Ok                                                        |
|                              | Description: All power domains are healthy.                       |
|                              | Throttle Threshold: 150 watts                                     |
|                              | Custom Threshold: -1                                              |
+------------------------------+-------------------------------------------------------------------+
| GPU Memory                   | Status: Ok                                                        |
|                              | Description: All memory channels are healthy.                     |
+------------------------------+-------------------------------------------------------------------+
```
 
# Dump the device statistics
Help info of the device statistics dump.
```
./xpumcli dump

Dump the device statistics

Usage: xpumcli dump [Options]
  xpumcli dump -d [deviceId] -t [deviceTileId] -m [metricsIds] -i [timeInterval] -n [dumpTimes]
  xpumcli dump -d [deviceId] -t [deviceTileId] -m [metricsIds] -i [timeInterval] -n [dumpTimes] -f [filename]
  xpumcli dump -stop [taskId]

optional arguments:
  -h,--help                   Print this help message and exit
  -d,--device                 The device id to query
  -t,--tile                   The device tile ID to query
  -m,--metrics                Metrics type to collect raw data, options:
                                0. GPU Utilization (%), GPU active time of the elapsed time
                                1. GPU Power (W)
                                2. GPU Frequency (MHz)
                                3. GPU Core Temperature (°C)
                                4. GPU Memory Temperature (°C)
                                5. GPU Memory Utilization (%)
                                6. GPU Memory Read (kB/s)
                                7. GPU Memory Write (kB/s)
                                8. GPU Energy Consumed (J)
                                9. GPU EU Array Active (%), the normalized sum of all cycles on all EUs that were spent actively executing instructions.
                                10. GPU EU Array Stall (%), the normalized sum of all cycles on all EUs during which the EUs were stalled. At least one thread is loaded, but the EU is stalled.
                                11. GPU EU Array Idle (%), the normalized sum of all cycles on all cores when no threads were scheduled on a core.
                                12. Reset Count
                                13. Programming Errors
                                14. Driver Errors
                                15. Cache Erros Correctable
                                16. Cache Errors Uncorrectable
  -i                          The interval (in seconds) to dump the device statistics. The interval will be XPU Manager sampling period if this parameter is not specified. 
  -n                          Number of the device statistics dump. The dump will never be ended if this parameter is not specified. 
  -f                          File to dump the device statistics. It is executed in background and the task ID is returned when the command is executed. 
  --stop                      Stop the dump task in background. 
  --list                      List the running dump tasks in background.
```

Dump the devce statistics on screen.
```
./xpumcli dump -d 0 -t 0 -m 0,1,2 -i 1 -n 5
Timestamp,DeviceId,TileId,GPU Utilization (%),GPU Power (W),GPU Frequency (MHz)
2021-11-08 13:31:43.100, 00, 0, 000,    , 0300
2021-11-08 13:31:44.100, 00, 0, 000,    , 0300
2021-11-08 13:31:45.100, 00, 0, 046,    , 1100
2021-11-08 13:31:46.100, 00, 0, 000,    , 0300
2021-11-08 13:31:47.100, 00, 0, 000,    , 0300
```

Dump the devce statistics to file.
```
./xpumcli dump -d 0 -t 0 -m 0,1,2 -f gpu_data.csv
Task id: 0. Dump device statistics to the file, gpu_data.csv.
```

List the dump tasks.
```
./xpumcli dump --list
Dump task 0 is running. 
```

Stop the dump task.
```
./xpumcli dump --stop 0
Dump task 0 is stopped. 
```

<!---
./xpumcli config -d 0
+-------------+-------------------+----------------------------------------------------------------+
| Device Type | Device Id/Tile Id | Configuration                                                  |
+-------------+-------------------+----------------------------------------------------------------+
| GPU         | 0                 | Power Limit(w): 300.0                                          |
|             |                   |   Supported Values: 0 to 500                                   |
|             |                   | Power Average Window (ms): 0                                   |
|             |                   |   Supported Values: 0 to 10000                                 |
+-------------+-------------------+----------------------------------------------------------------+
| GPU         | 0/0               | GPU Min Frequency(MHz): 300.0                                  |
|             |                   | GPU Max Frequency(MHz): 1400.0                                 |
|             |                   |   Supported Values: 300, 350, 400, 450, 500, 550, 600, 650     |
|             |                   |   700, 750, 800, 850, 900, 950, 1000, 1050, 1100, 1150, 1200   |
|             |                   |   1250, 1300                                                   |
|             |                   |                                                                |
|             |                   | Standby type: default                                          |
|             |                   |     Supported Values: default, never                           |
|             |                   |                                                                |
|             |                   | Scheduler Mode: timeslice                                      |
|             |                   |     Supported Values: timeout, timeslice, exclusive            |
+-------------+-------------------+----------------------------------------------------------------+
| GPU         | 0/0               | GPU Min Frequency(MHz): 300.0                                  |
|             |                   | GPU Max Frequency(MHz): 1400.0                                 |
|             |                   |   Supported Values: 300, 350, 400, 450, 500, 550, 600, 650     |
|             |                   |   700, 750, 800, 850, 900, 950, 1000, 1050, 1100, 1150, 1200   |
|             |                   |   1250, 1300                                                   |
|             |                   |                                                                |
|             |                   | Standby type: default                                          |
|             |                   |     Supported Values: default, never                           |
|             |                   |                                                                |
|             |                   | Scheduler Mode: timeslice                                      |
|             |                   |     Supported Values: timeout, timeslice, exclusive            |
+-------------+-------------------+----------------------------------------------------------------+
--->




