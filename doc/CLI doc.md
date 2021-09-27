# Help info

```
$ xpumcli --help

Usage: 
    -h, --help              Show this help message and exit
    -v, --version           Display version information and exit
    xpumcli <subcommand>
        -h, --help          For more info about <subcommand>

Subcommand options:
    discovery               Discover GPUs on the system
    group                   Group management
    health                  Display health status of GPUs
    events                  GPU events management
    config                  GPU configuration management
    fwflash                 Flash GPU firmware
    diag                    System validation/diagnostic
    agentset                XPUM agent settings
    stats                   Display device statistics
    telemetry               Collect and dump metrics raw data
    reset                   Reset the device
	

```

```
$ xpumcli --version
CLI:
    Version: 1.0.0
    Build Id: 10

Service:
    Version: 1.0.0
    Build Id: 10
    Level Zero Version: 1.2.13
```


# Discovery subcommand
## Help info
```
$ xpumcli discovery --help

discovery -- Used to discover and identify GPUs and their attributes.

Usage: xpumcli discovery [-h] [-l] [-d]

Optional arguments:
    -h, --help              Show this help message and exit
    -l, --list              List all devices discovered on the host
    -d <deviceId>, --device <deviceId> 
                            The device id to query, will show more detailed info
```

## List GPUs
```
$ xpumcli discovery -l
Device Type: GPU
+-----------+--------------------------------------------------------+
| Device Id | Device Information                                     |
+-----------+--------------------------------------------------------+
| 0         | Device Name: Intel(R) Iris(R) Xe MAX Graphics [0x4905] |
|           | Vendor Name: Intel(R) Corporation                      |
|           | UUID: 00000000-0000-0000-0000-490500008086             |
|           | PCI BDF Address: 0000:b1:0.0                           |
+-----------+--------------------------------------------------------+
| 1         | Device Name: Intel(R) Iris(R) Xe MAX Graphics [0x4905] |
|           | Vendor Name: Intel(R) Corporation                      |
|           | UUID: 00000000-0000-0001-0000-490500008086             |
|           | PCI BDF Address: 0000:1a:0.0                           |
+-----------+--------------------------------------------------------+
```
```
$ xpumcli discovery -l -d 0
+-----------+--------------------------------------------------------+
| Device Id | Device Information                                     |
+-----------+--------------------------------------------------------+
| 0         | Device Type: GPU                                       |
|           | Device Name: Intel(R) Iris(R) Xe MAX Graphics [0x4905] |
|           |                                                        |
|           | Vendor Name: Intel(R) Corporation                      |
|           | UUID: 00000000-0000-0000-0000-490500008086             |
|           | PCI BDF Address: 0000:b1:0.0                           |
|           |                                                        |
|           | Driver Version: 16796698                               |
|           | Firmware Name: GSC                                     |
|           | Firmware Version: ATS1_0.7178                          |
|           |                                                        |
|           | Number of Sub Devices: 2                               |
|           | Serial Number: Unknown                                 |
|           | Core Clock Rate: 1300 MHz                              |
|           | Max Mem Alloc Size: 4095 MiB                           |
|           | Max Hardware Contexts: 65536                           |
|           | Max Command Queue Priority: 0                          |
|           |                                                        |
|           | Number of Slices: 2                                    |
|           | Number of Sub Slices Per Slice: 30                     |
|           | Number of EUs Per Sub Slice: 16                        |
|           | Number of Threads Per EU: 8                            |
|           | Pysical EU SIMD Width: 8                               |
|           | Timer Resolution: 80                                   |
|           | Timestamp Valid Bits: 36                               |
|           | PCI Vendor Id: 0x8086                                  |
|           | Kernel Timestamp Valid Bits: 32                        |
|           | Flags: 0                                               |
|           | Memory Physical Size: 32608.0 MiB                      |
|           | Memory Free Size: 32575 MiB                            |
+-----------+--------------------------------------------------------+
```

# Group subcommand
## Help info
```
$ xpumcli group --help

group -- Used to manage device groups.

Usage: xpumcli group [-h] [-c] [-r] [-l] [-g] [-a] [-d]

Optional arguments:
    -h, --help              Show this help message and exit
    -c <groupName>, --create <groupName> 
                            Create a group
    -r, --remove            Remove a group
    -l, --list              Display info for a group or all groups
    -g <groupId>, --groupId <groupId>    
                            The id of the group to manipulate
    -a <deviceIds>, --add <deviceIds>    
                            Add devices to a group
    -d <deviceIds>, --delete <deviceIds> 
                            Delete devices from a group
```

## Create group
```
$ xpumcli group -c "All Gpus"
+----------+------------------------+
| Group Id | Group Properties       |
+----------+------------------------+
| 0        | Group Name: "All Gpus" |
|          | Device Ids: []         |
+----------+------------------------+
```

## Add a device to a group
```
$ xpumcli group -g 0 -a 0 1
+----------+------------------------+
| Group Id | Group Properties       |
+----------+------------------------+
| 0        | Group Name: "All Gpus" |
|          | Device Ids: [0,1]      |
+----------+------------------------+
```

## Delete a device from a group
```
$ xpumcli group -g 0 -d 0
+----------+------------------------+
| Group Id | Group Properties       |
+----------+------------------------+
| 0        | Group Name: "All Gpus" |
|          | Device Ids: [1]        |
+----------+------------------------+
```

## Display info about a group
```
$ xpumcli group -g 0 -l
+----------+------------------------+
| Group Id | Group Properties       |
+----------+------------------------+
| 0        | Group Name: "All Gpus" |
|          | Device Ids: [1]        |
+----------+------------------------+
```

## Display info about all groups
```
$ xpumcli group -l
+----------+------------------------+
| Group Id | Group Properties       |
+----------+------------------------+
| 0        | Group Name: "All Gpus" |
|          | Device Ids: [0,1]      |
+----------+------------------------+
| 1        | Group Name: "A Group"  |
|          | Device Ids: [1]        |
+----------+------------------------+
```

## Remove a group
```
$ xpumcli group -r -g 0
```

# Health subcommmand
## Help info
```
$ xpumcli health --help

health -- used to display device health status

Usage: xpumcli health [-h] [-d] [-g] [-t]

Optional arguments:
    -h, --help              Show this help message and exit
    -l, --list              Display health info for all devices
    -d <deviceId>, --device <deviceId>    
                            The device id to query
    -g <groupId>, --group <groupId>       
                            The group id to query
    -t <threshold>, --thold <threshold>   
                            Set temperature threshold for device
```

## Display health
```
$ xpumcli health -d 0
+-------------+-------------------------------+
| Device Type | GPU                           |
+-------------+-------------+-----------------+
| Device Id   | Category    | Health          |
+-------------+-------------+-----------------+
| 0           | Temperature | Status: Warning |
|             |             | Threshold: 90   |
|             +-------------+-----------------+
|             | Memory      | Status: OK      |
|             +-------------+-----------------+
|             | Fabric Port | Status: OK      |
+-------------+-------------+-----------------+
```

```
$ xpumcli health -g 0
+-------------+-------------------------------+
| Device Type | GPU                           |
+-------------+-------------+-----------------+
| Device Id   | Category    | Health          |
+-------------+-------------+-----------------+
| 0           | Temperature | Status: Warning |
|             |             | Threshold: 90   |
|             +-------------+-----------------+
|             | Memory      | Status: OK      |
|             +-------------+-----------------+
|             | Fabric Port | Status: OK      |
+-------------+-------------+-----------------+
| 1           | Temperature | Status: Warning |
|             |             | Threshold: 90   |
|             +-------------+-----------------+
|             | Memory      | Status: OK      |
|             +-------------+-----------------+
|             | Fabric Port | Status: OK      |
+-------------+-------------+-----------------+
```

## Set temperature threshold
```
$ xpumcli health -d 0 -t 85
+-------------+-------------------------------+
| Device Type | GPU                           |
+-------------+-------------+-----------------+
| Device Id   | Category    | Health          |
+-------------+-------------+-----------------+
| 0           | Temperature | Status: Warning |
|             |             | Threshold: 85   |
|             +-------------+-----------------+
|             | Memory      | Status: OK      |
|             +-------------+-----------------+
|             | Fabric Port | Status: OK      |
+-------------+-------------+-----------------+
```

```
$ xpumcli health -g 0 -t 85
+-------------+-------------------------------+
| Device Type | GPU                           |
+-------------+-------------+-----------------+
| Device Id   | Category    | Health          |
+-------------+-------------+-----------------+
| 0           | Temperature | Status: Warning |
|             |             | Threshold: 85   |
|             +-------------+-----------------+
|             | Memory      | Status: OK      |
|             +-------------+-----------------+
|             | Fabric Port | Status: OK      |
+-------------+-------------+-----------------+
| 1           | Temperature | Status: Warning |
|             |             | Threshold: 85   |
|             +-------------+-----------------+
|             | Memory      | Status: OK      |
|             +-------------+-----------------+
|             | Fabric Port | Status: OK      |
+-------------+-------------+-----------------+
```

# Events subcommand
## Help info
```
$ xpumcli events --help

events -- used to display device events

Usage: xpumcli events [-h] [-l] [-d] [-g] [-c]

Optional arguments:
    -h, --help              Show this help message and exit
    -l, --list              Display events info for all devices
    -d <deviceId>, --device <deviceId>      
                            The device id to query
    -g <groupId>, --group <groupId>         
                            The group id to query
    -c, --clear             Clear all events
```

## Display events
```
$ xpumcli events -d 0
+------------------------+-----------------+---------------+-------------------------+-------------+
| Event Index            | Device Type     | Device Id     | Time                    | Severity    |
+------------------------+-----------------+---------------+-------------------------+-------------+
| Event Type             | Description                                                             |
+------------------------+-----------------+---------------+-------------------------+-------------+
| 0                      | GPU             | 0             | 2021-07-09 16:03:36     | Critical    |
+------------------------+-----------------+---------------+-------------------------+-------------+
| Thermal over threshold | GPU temperature is 81 Celsius degree, higher than critical threshold 80 |
+------------------------+-------------------------------------------------------------------------+
```

```
$ xpumcli events -g 0
+------------------------+-----------------+---------------+-------------------------+-------------+
| Event Index            | Device Type     | Device Id     | Time                    | Severity    |
+------------------------+-----------------+---------------+-------------------------+-------------+
| Event Type             | Description                                                             |
+------------------------+-----------------+---------------+-------------------------+-------------+
| 0                      | GPU             | 0             | 2021-07-09 16:03:36     | Critical    |
+------------------------+-----------------+---------------+-------------------------+-------------+
| Thermal over threshold | GPU temperature is 81 Celsius degree, higher than critical threshold 80 |
+------------------------+-------------------------------------------------------------------------+
```

# Config subcommand
## Help info
```
$ xpumcli config --help

config -- used to configure settings for devices.

Usage: xpumcli config [-h] [-d] [-g] [-l]

Optional arguments:
    -h, --help              Show this help message and exit
    -d <deviceId>, --device <deviceId>      
                            The device id to query
    -l, --list              List all configurations
    -- <key>=<value>        Set device configurations
```
## List configurations
```
$ xpumcli config -l -d 0 
+-------------+-------------------+--------------------------------------------+
| Device Type | Device Id/Tile ID | Configuration                              |
+-------------+-------------------+--------------------------------------------+
| GPU         | 0/0               | Power Limit(w): 320.0                      |
|             |                   |     Legal Range: 0 to 500                  |
|             |                   | Power Average Window (ms): 124             |
|             |                   |     Legal Range: 0 to 125                  |
|             |                   |                                            |
|             |                   | GPU Min Frequency(MHz): 370.0              |
|             |                   | GPU Max Frequency(MHz): 1250.0             |
|             |                   |     Legal Range: 300, 350, 400, 450, 500,  |
|             |                   | 550, 600, 650, 700, 750, 800, 850, 900,    |
|             |                   | 950, 1000,1050,1100, 1150, 1200, 1250,     |
|             |                   | 1300                                       |
|             |                   |                                            |
|             |                   | Standby type: NEVER                        |
|             |                   |     Legal Options: Default, Never          |
|             |                   |                                            |
|             |                   | Scheduler Mode: TIMESLICE                  |
|             |                   |     Legal Options: Timeout,Timeslice,      |
|             |                   | Exclusive                                  |
+-------------+-------------------+--------------------------------------------+
| GPU         | 0/1               | Power Limit(w): 320.0                      |
|             |                   |     Legal Range: 0 to 500                  |
|             |                   | Power Average Window (ms): 124             |
|             |                   |     Legal Range: 0 to 125                  |
|             |                   |                                            |
|             |                   | GPU Min Frequency(MHz): 300.0              |
|             |                   | GPU Max Frequency(MHz): 300.0              |
|             |                   |     Legal Range: 300, 350, 400, 450, 500,  |
|             |                   | 550, 600, 650, 700, 750, 800, 850, 900,    |
|             |                   | 950, 1000,1050,1100, 1150, 1200, 1250,     |
|             |                   | 1300                                       |
|             |                   |                                            |
|             |                   | Standby type: NEVER                        |
|             |                   |     Legal Options: Default, Never          |
|             |                   |                                            |
|             |                   | Scheduler Mode: EXCLUSIVE                  |
|             |                   |     Legal Options: Timeout,Timeslice,      |
|             |                   | Exclusive                                  |
+-------------+-------------------+--------------------------------------------+

$ xpumcli config -l -d 0 -t 0
+-------------+-------------------+--------------------------------------------+
| Device Type | Device Id/Tile ID | Configuration                              |
+-------------+-------------------+--------------------------------------------+
| GPU         | 0/0               | Power Limit(w): 300.0                      |
|             |                   |     Legal Range: 0 to 500                  |
|             |                   | Power Average Window (ms): 0               |
|             |                   |     Legal Range: 0 to 125                  |
|             |                   |                                            |
|             |                   | GPU Min Frequency(MHz): 370.0              |
|             |                   | GPU Max Frequency(MHz): 1250.0             |
|             |                   |     Legal Range: 300, 350, 400, 450, 500,  |
|             |                   | 550, 600, 650, 700, 750, 800, 850, 900,    |
|             |                   | 950, 1000,1050,1100, 1150, 1200, 1250,     |
|             |                   | 1300                                       |
|             |                   |                                            |
|             |                   | Standby type: NEVER                        |
|             |                   |     Legal Options: Default, Never          |
|             |                   |                                            |
|             |                   | Scheduler Mode: TIMESLICE                  |
|             |                   |     Legal Options: Timeout,Timeslice,      |
|             |                   | Exclusive                                  |
+-------------+-------------------+--------------------------------------------+
```
## Set configurations
```
$ xpumcli config -d 0 -t 0 --"Scheduler"=Timeout,50000
+-------------+-------------------+--------------------------------------------+
| Device Type | Device Id/Tile ID | Configuration                              |
+-------------+-------------------+--------------------------------------------+
| GPU         | 0/0               | Power Limit(w): 300.0                      |
|             |                   |     Legal Range: 0 to 500                  |
|             |                   | Power Average Window (ms): 0               |
|             |                   |     Legal Range: 0 to 125                  |
|             |                   |                                            |
|             |                   | GPU Min Frequency(MHz): 370.0              |
|             |                   | GPU Max Frequency(MHz): 1250.0             |
|             |                   |     Legal Range: 300, 350, 400, 450, 500,  |
|             |                   | 550, 600, 650, 700, 750, 800, 850, 900,    |
|             |                   | 950, 1000,1050,1100, 1150, 1200, 1250,     |
|             |                   | 1300                                       |
|             |                   |                                            |
|             |                   | Standby type: NEVER                        |
|             |                   |     Legal Options: Default, Never          |
|             |                   |                                            |
|             |                   | Scheduler Mode: TIMEOUT                    |
|             |                   |     Legal Options: Timeout,Timeslice,      |
|             |                   | Exclusive                                  |
+-------------+-------------------+--------------------------------------------+

$ xpumcli config -d 0 -t 0 --"Scheduler"=Exclusive
+-------------+-------------------+--------------------------------------------+
| Device Type | Device Id/Tile ID | Configuration                              |
+-------------+-------------------+--------------------------------------------+
| GPU         | 0/0               | Power Limit(w): 300.0                      |
|             |                   |     Legal Range: 0 to 500                  |
|             |                   | Power Average Window (ms): 0               |
|             |                   |     Legal Range: 0 to 125                  |
|             |                   |                                            |
|             |                   | GPU Min Frequency(MHz): 370.0              |
|             |                   | GPU Max Frequency(MHz): 1250.0             |
|             |                   |     Legal Range: 300, 350, 400, 450, 500,  |
|             |                   | 550, 600, 650, 700, 750, 800, 850, 900,    |
|             |                   | 950, 1000,1050,1100, 1150, 1200, 1250,     |
|             |                   | 1300                                       |
|             |                   |                                            |
|             |                   | Standby type: NEVER                        |
|             |                   |     Legal Options: Default, Never          |
|             |                   |                                            |
|             |                   | Scheduler Mode: EXCLUSIVE                  |
|             |                   |     Legal Options: Timeout,Timeslice,      |
|             |                   | Exclusive                                  |
+-------------+-------------------+--------------------------------------------+

$ xpumcli config -d 0 -t 0 --"Scheduler"=Timeslice,10000,50000
+-------------+-------------------+--------------------------------------------+
| Device Type | Device Id/Tile ID | Configuration                              |
+-------------+-------------------+--------------------------------------------+
| GPU         | 0/0               | Power Limit(w): 300.0                      |
|             |                   |     Legal Range: 0 to 500                  |
|             |                   | Power Average Window (ms): 0               |
|             |                   |     Legal Range: 0 to 125                  |
|             |                   |                                            |
|             |                   | GPU Min Frequency(MHz): 370.0              |
|             |                   | GPU Max Frequency(MHz): 1250.0             |
|             |                   |     Legal Range: 300, 350, 400, 450, 500,  |
|             |                   | 550, 600, 650, 700, 750, 800, 850, 900,    |
|             |                   | 950, 1000,1050,1100, 1150, 1200, 1250,     |
|             |                   | 1300                                       |
|             |                   |                                            |
|             |                   | Standby type: NEVER                        |
|             |                   |     Legal Options: Default, Never          |
|             |                   |                                            |
|             |                   | Scheduler Mode: TIMESLICE                  |
|             |                   |     Legal Options: Timeout,Timeslice,      |
|             |                   | Exclusive                                  |
+-------------+-------------------+--------------------------------------------+

$ xpumcli config -d 0 -t 0  --"PowerLimit"=330,120
+-------------+-------------------+--------------------------------------------+
| Device Type | Device Id/Tile ID | Configuration                              |
+-------------+-------------------+--------------------------------------------+
| GPU         | 0/0               | Power Limit(w): 330.0                      |
|             |                   |     Legal Range: 0 to 500                  |
|             |                   | Power Average Window (ms): 120             |
|             |                   |     Legal Range: 0 to 125                  |
|             |                   |                                            |
|             |                   | GPU Min Frequency(MHz): 370.0              |
|             |                   | GPU Max Frequency(MHz): 1250.0             |
|             |                   |     Legal Range: 300, 350, 400, 450, 500,  |
|             |                   | 550, 600, 650, 700, 750, 800, 850, 900,    |
|             |                   | 950, 1000,1050,1100, 1150, 1200, 1250,     |
|             |                   | 1300                                       |
|             |                   |                                            |
|             |                   | Standby type: NEVER                        |
|             |                   |     Legal Options: Default, Never          |
|             |                   |                                            |
|             |                   | Scheduler Mode: TIMESLICE                  |
|             |                   |     Legal Options: Timeout,Timeslice,      |
|             |                   | Exclusive                                  |
+-------------+-------------------+--------------------------------------------+

$ xpumcli config -d 0 -t 0 --"FrequencyRange"=400,1200
+-------------+-------------------+--------------------------------------------+
| Device Type | Device Id/Tile ID | Configuration                              |
+-------------+-------------------+--------------------------------------------+
| GPU         | 0/0               | Power Limit(w): 330.0                      |
|             |                   |     Legal Range: 0 to 500                  |
|             |                   | Power Average Window (ms): 120             |
|             |                   |     Legal Range: 0 to 125                  |
|             |                   |                                            |
|             |                   | GPU Min Frequency(MHz): 400.0              |
|             |                   | GPU Max Frequency(MHz): 1200.0             |
|             |                   |     Legal Range: 300, 350, 400, 450, 500,  |
|             |                   | 550, 600, 650, 700, 750, 800, 850, 900,    |
|             |                   | 950, 1000,1050,1100, 1150, 1200, 1250,     |
|             |                   | 1300                                       |
|             |                   |                                            |
|             |                   | Standby type: NEVER                        |
|             |                   |     Legal Options: Default, Never          |
|             |                   |                                            |
|             |                   | Scheduler Mode: TIMESLICE                  |
|             |                   |     Legal Options: Timeout,Timeslice,      |
|             |                   | Exclusive                                  |
+-------------+-------------------+--------------------------------------------+

$ xpumcli config -d 0 -t 0 --"Standby"=Never
+-------------+-------------------+--------------------------------------------+
| Device Type | Device Id/Tile ID | Configuration                              |
+-------------+-------------------+--------------------------------------------+
| GPU         | 0/0               | Power Limit(w): 330.0                      |
|             |                   |     Legal Range: 0 to 500                  |
|             |                   | Power Average Window (ms): 120             |
|             |                   |     Legal Range: 0 to 125                  |
|             |                   |                                            |
|             |                   | GPU Min Frequency(MHz): 400.0              |
|             |                   | GPU Max Frequency(MHz): 1200.0             |
|             |                   |     Legal Range: 300, 350, 400, 450, 500,  |
|             |                   | 550, 600, 650, 700, 750, 800, 850, 900,    |
|             |                   | 950, 1000,1050,1100, 1150, 1200, 1250,     |
|             |                   | 1300                                       |
|             |                   |                                            |
|             |                   | Standby type: NEVER                        |
|             |                   |     Legal Options: Default, Never          |
|             |                   |                                            |
|             |                   | Scheduler Mode: TIMESLICE                  |
|             |                   |     Legal Options: Timeout,Timeslice,      |
|             |                   | Exclusive                                  |
+-------------+-------------------+--------------------------------------------+
```

# Firmware flash subcommand
## Help info
```
$ xpumcli fwflash --help

fwflash -- used to update firmware for devices.

Usage: xpumcli fwflash [-h] [-d] [-g] [-l] [-u]

Optional arguments:
    -h, --help              Show this help message and exit
    -d <deviceId>, --device <deviceId>      
                            The device id to flash firmware
    -t <firmwareName>, --type <firmwareName>
                            Firmware name to flash
    -f <imagePath>, --file <imagePath>           
                            The file path in the server, used to flash firmware, 
                            should be located in /usr/bin
```

## Flash firmware

```
$ xpumcli fwflash -d 0 t "GSC" -f "/tmp/firmware.1"
Start flashing firmware:
Firmware name: GSC
Image path: /tmp/firmware.1
...
Firmware successfully flashed!
```

```
// Mean while run another firmware update task

$ xpumcli fwflash -d 0 t "GSC" -f "/tmp/firmware.1"
Error: firmware update is already running!
```

# Diag subcommand
## Help info
```
$ xpumcli diag --help

diag -- Used to run diagnostics on the system

Usage: xpumcli diag [-h] [-d] [-g] [-l] [-L]

Optional arguments:
    -h, --help              Show this help message and exit
    -d <deviceId>, --device <deviceId>      
                            The device id to query
    -g <groupId>, --group <groupId>         
                            The group id to query
    -L, --Level             The diagnostics level to run
```

## Run diagnostics task
```
$ xpumcli diag -d 0 -L 1
+-------------+-----------+----------------------------------------+
| Device Type | Device Id | Diagnostics Task                       |
+-------------+-----------+----------------------------------------+
| GPU         | 0         | Diagnostics Level: 1                   |
|             |           | Diagnostics Time: 2021-07-09 16:03:36  |
|             |           | Memory Status:                         |
|             |           |     Status: OK                         |
|             |           |     Description: "Bandwidth: 80GBps"   |
|             |           | PCIe Speed Status:                     |
|             |           |     Status: OK                         |
|             |           |     Description: "Speed: 1200Mbps"     |
+-------------+-----------+----------------------------------------+
```

```
// Mean while run another diagnostics task

$ xpumcli diag -t GPU -d 0 -L 1
Error: Diagnostics is already running!
```

# Agentset subcommand
## Help info
```
$ xpumcli agentset --help

agentset -- Used to set agent settings

Usage: xpumcli agentset [-h] [-t] [-n]

Optional arguments:
    -h, --help              Show this help message and exit
    -l, --list              Display all agent settings
    -t <interval>, --time <interval>        
                            Set the time interval(in milliseoncds) 
                                by which xpum daemon tries to get 
                                data. Options are 100,200,500,1000.
    -n <events num>, --num <events num>     
                            The limit count of events will be 
                                stored by xpum daemon. Default 1000
```

## Set agent settings
```
$ xpumcli agentset -t 500 -p 50
Successful!

$ xpumcli agentset -l
+--------------------+-------+
| Name               | Value |
+--------------------+-------+
| Sampling Interval  | 500ms |
+--------------------+-------+
| Events Num Limit   | 1000  |
+--------------------+-------+
```

# Stats subcommand

## Help info
```
$ xpumcli stats --help

stats -- Used to display detailed device statistics data from last query to now

Usage: xpumcli stats [-h] [-g] [-d] [-t]

Optional arguments:
    -h, --help              Show this help message and exit
    -d <deviceId>, --device <deviceId>  
                            The device id to query
    -g <groupId>, --group <groupId>     
                            The group id to query
    -t, --tile              Show per tile data or not
```
## List stats
```
$ xpumcli stats
+---------------------+---------------------+
| Device Id           | 0                   |
+---------------------+---------------------+
| Start Time          | 2021-07-09 16:03:36 |
| End Time            | 2021-07-09 19:03:36 |
| Energy Consumed (J) | 2000                |
+---------------------+---------------------+
| GPU_Computation (%) | avg: 70             |
|                     | min: 70             |
|                     | max: 70             |
+---------------------+---------------------+
| Occupation (%)      | avg: 70             |
|                     | min: 70             |
|                     | max: 70             |
+---------------------+---------------------+
| GPU_Frequency (MHz) | avg: 500            |
|                     | min: 300            |
|                     | max: 700            |
+---------------------+---------------------+
| Device Id           | 1                   |
+---------------------+---------------------+
| Start Time          | 2021-07-09 16:03:36 |
| End Time            | 2021-07-09 19:03:36 |
| Energy Consumed (J) | 2000                |
+---------------------+---------------------+
| GPU_Computation (%) | avg: 70             |
|                     | min: 70             |
|                     | max: 70             |
+---------------------+---------------------+
| Occupation (%)      | avg: 70             |
|                     | min: 70             |
|                     | max: 70             |
+---------------------+---------------------+
| GPU_Frequency (MHz) | avg: 500            |
|                     | min: 300            |
|                     | max: 700            |
+---------------------+---------------------+

$ xpumcli stats -d 0 -t
Device Type: GPU
+---------------------+---------------------+
| Device Id           | 0                   |
| Tile                | 0                   |
+---------------------+---------------------+
| Start Time          | 2021-07-09 16:03:36 |
| End Time            | 2021-07-09 19:03:36 |
| Energy Consumed (J) | 2000                |
+---------------------+---------------------+
| GPU_Computation (%) | avg: 70             |
|                     | min: 70             |
|                     | max: 70             |
+---------------------+---------------------+
| Occupation (%)      | avg: 70             |
|                     | min: 70             |
|                     | max: 70             |
+---------------------+---------------------+
| GPU_Frequency (MHz) | avg: 500            |
|                     | min: 300            |
|                     | max: 700            |
+---------------------+---------------------+
| Device Id           | 0                   |
| Tile                | 1                   |
+---------------------+---------------------+
| Start Time          | 2021-07-09 16:03:36 |
| End Time            | 2021-07-09 19:03:36 |
| Energy Consumed (J) | 2000                |
+---------------------+---------------------+
| GPU_Computation (%) | avg: 70             |
|                     | min: 70             |
|                     | max: 70             |
+---------------------+---------------------+
| Occupation (%)      | avg: 70             |
|                     | min: 70             |
|                     | max: 70             |
+---------------------+---------------------+
| GPU_Frequency (MHz) | avg: 500            |
|                     | min: 300            |
|                     | max: 700            |
+---------------------+---------------------+
```

# Telemetry subcommmand
## Help info
```
$ xpumcli telemetry --help

telemetry -- used to collect and dump metrics raw data

Usage: xpumcli telemetry [-h] [start] [-d] [-g] [-t] [-m] [stop] [dump]

Optional arguments:
    -h, --help              Show this help message and exit
    start                   Start metrics raw data collect task, will return a task id
                                Notice XPU Manager supports at most 16 tasks coexist.
                                When limit reached, start a new task will cause the oldest task 
                                automatically terminated and data removed. 
    -t, --tile              Collect metrics data at tile level
    -m <metricsIds>, --metrics <metricsIds>           
                            Metrics type to collect raw data, options:
                                0. GPU Utilization
                                1. Occupation
                                2. Issue Efficiency
                                3. Execution Efficiency
                                4. Non Occupation
                                5. Power
                                6. Energy
                                7. Gpu Frequency
                                8. Gpu Temperature
                                9. Memory Used
                                10. Fabric Speed
                                11. GtiReadThroughput
                                12. GtiWriteThroughput
    -d <deviceId>, --device <deviceId>  
                            The device id to query
    -g <groupId>, --group <groupId>     
                            The group id to query
    stop <taskId>           Stop corresponding collect task. 
                                Notice a task collect 5000 time frames at most.
                                When limit reached, the task will be terminated automatically.
    dump <taskId>           Dump raw datas that collected in <taskId>, in csv format
```

## Start telemetry task
```
$ xpumcli telemetry start -m 0 5 7 8 9 -d 0 --tile
Start telemetry successfully!
Task id: 0

$ xpumcli telemetry start -m 1 2 3 -g 0
Start telemetry successfully!
Task id: 1
```

## Stop telemetry task
```
$ xpumcli telemetry stop 0
Stop telemetry successfully!
```

## Dump telemetry data
```
$ xpumcli telemetry dump 0
Timestamp,DeviceId,TileId,Power(W),GPU Frequency(MHz),GPU Temperature(°C),Memory Used(MiB),GPU Utilization(%)
2021-07-09 16:03:36,0,0,71,300,35,32.8,0
2021-07-09 16:03:37,0,0,71,300,35,32.8,0
2021-07-09 16:03:38,0,0,71,300,35,32.8,0
2021-07-09 16:03:39,0,0,71,300,35,32.8,0
2021-07-09 16:03:40,0,0,71,300,35,32.8,0
```
# Reset device
## Help info
```
usage: xpumcli reset [-h] [-d <deviceId>]

optional arguments:
  -h, --help            show this help message and exit
  -d <deviceId>, --device <deviceId>
                        reset device
## forcely reset device
```
$ xpumcli reset -d 0
OK
```