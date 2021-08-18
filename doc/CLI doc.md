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
    telemetry               Display telemetry data of GPUs
    health                  Display health status of GPUs
    events                  GPU events management
    config                  GPU configuration management
    firmwareupdate          Update GPU firmware
    diag                    System validation/diagnostic
    agentset                XPUM agent settings
    stats                   Display device statistics

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
    -d <deviceIds>, --device <deviceIds> 
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
$ xpumcli group -g 0 -a 0,1
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

# Telemetry subcommmand
## Help info
```
$ xpumcli telemetry --help

telemetry -- used to display device telemetry data

Usage: xpumcli telemetry [-h] [-i] [-d] [-g]

Optional arguments:
    -h, --help              Show this help message and exit
    -l, --list              Display telemetry info for all devices
    -i, --immed             Get realtime data
    -d <deviceId>, --device <deviceId>  
                            The device id to query
    -g <groupId>, --group <groupId>     
                            The group id to query
    -o, --output            Export telemetried raw data of one aggregate 
                                period, which can be set by "agentset" 
                                subcommand. Raw data means not aggregated. 
                                Output will be written to standard output
                                in csv format.
```

## Display telemetry
```
$ xpumcli telemetry -d 0
+-------------+-----------------------------------------+
| Device Type | GPU                                     |
+-------------+------------------------+----------------+
| Device Id   | Category               | Telemetry      |
+-------------+------------------------+----------------+
| 0           | Power(W)               | current: 71    |
|             |                        | avg: 71        |
|             |                        | max: 71        |
|             |                        | min: 71        |
|             +------------------------+----------------+
|             | GPU Frequency(MHz)     | current: 300   |
|             |                        | avg: 300       |
|             |                        | max: 300       |
|             |                        | min: 300       |
|             +------------------------+----------------+
|             | GPU Temperature(°C)    | current: 35    |
|             |                        | avg: 36        |
|             |                        | max: 36        |
|             |                        | min: 35        |
|             +------------------------+----------------+
|             | Memory Used(MiB)       | current: 32.80 |
|             |                        | avg: 32.80     |
|             |                        | max: 32.81     |
|             |                        | min: 32.80     |
|             +------------------------+----------------+
|             | GPU Utilization(%)     | current: 0     |
|             |                        | avg: 0         |
|             |                        | max: 0         |
|             |                        | min: 0         |
|             +------------------------+----------------+
|             | Fabric Port Speed(bps) | current: 0     |
|             |                        | avg: 0         |
|             |                        | max: 0         |
|             |                        | min: 0         |
+-------------+------------------------+----------------+
```

```
$ xpumcli telemetry -i -d 0
+-------------+-----------------------------------------+
| Device Type | GPU                                     |
+-------------+------------------------+----------------+
| Device Id   | Category               | Telemetry      |
+-------------+------------------------+----------------+
| 0           | Power(W)               | current: 71    |
|             +------------------------+----------------+
|             | GPU Frequency(MHz)     | current: 300   |
|             +------------------------+----------------+
|             | GPU Temperature(°C)    | current: 35    |
|             +------------------------+----------------+
|             | Memory Used(MiB)       | current: 32.80 |
|             +------------------------+----------------+
|             | GPU Utilization(%)     | current: 0     |
|             +------------------------+----------------+
|             | Fabric Port Speed(bps) | current: 0     |
+-------------+------------------------+----------------+
```

```
$ xpumcli telemetry -i -g 0
+-------------+-----------------------------------------+
| Device Type | GPU                                     |
+-------------+------------------------+----------------+
| Device Id   | Category               | Telemetry      |
+-------------+------------------------+----------------+
| 0           | Power(W)               | current: 71    |
|             +------------------------+----------------+
|             | GPU Frequency(MHz)     | current: 300   |
|             +------------------------+----------------+
|             | GPU Temperature(°C)    | current: 35    |
|             +------------------------+----------------+
|             | Memory Used(MiB)       | current: 32.80 |
|             +------------------------+----------------+
|             | GPU Utilization(%)     | current: 0     |
|             +------------------------+----------------+
|             | Fabric Port Speed(bps) | current: 0     |
+-------------+------------------------+----------------+
| 1           | Power(W)               | current: 71    |
|             +------------------------+----------------+
|             | GPU Frequency(MHz)     | current: 300   |
|             +------------------------+----------------+
|             | GPU Temperature(°C)    | current: 35    |
|             +------------------------+----------------+
|             | Memory Used(MiB)       | current: 32.80 |
|             +------------------------+----------------+
|             | GPU Utilization(%)     | current: 0     |
|             +------------------------+----------------+
|             | Fabric Port Speed(bps) | current: 0     |
+-------------+------------------------+----------------+
```

```
$ xpumcli telemetry -d 0 -i -o
Timestamp,DeviceId,Power(W),GPU Frequency(MHz),GPU Temperature(°C),Memory Used(MiB),GPU Utilization(%),Fabric Port Speed(bps)
2021-07-09 16:03:36,0,71,300,35,32.8,0,0
2021-07-09 16:03:37,0,71,300,35,32.8,0,0
2021-07-09 16:03:38,0,71,300,35,32.8,0,0
2021-07-09 16:03:39,0,71,300,35,32.8,0,0
2021-07-09 16:03:40,0,71,300,35,32.8,0,0
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
    -g <groupId>, --group <groupId>         
                            The group id to query
    -l, --list              List all configurations
    -- <key>=<value>        Set device configurations
```
## List configurations
```
$ xpumcli config -l
+-------------+-----------+------------------------------+
| Device Type | Device Id | Configuration                |
+-------------+-----------+------------------------------+
| GPU         | 0         | Power Limit(w): 300          |
|             |           |     Legal Range: 0 to 500    |
|             |           |                              |
|             |           | GPU Frequency(MHz): 300,     |
|             |           |     Legal Range: 100 to 1500 |
|             |           |                              |
|             |           | Memory Frequency(MHz): 800   |
|             |           |     Legal Range: 200 to 1000 |
|             |           |                              |
|             |           | Performance Factor: 1        |
|             |           |     Legal Options: 1,2,3     |
|             +-----------+------------------------------+
|             | 1         | Power Limit(w): 300          |
|             |           |     Legal Range: 0 to 500    |
|             |           |                              |
|             |           | GPU Frequency(MHz): 300,     |
|             |           |     Legal Range: 100 to 1500 |
|             |           |                              |
|             |           | Memory Frequency(MHz): 800   |
|             |           |     Legal Range: 200 to 1000 |
|             |           |                              |
|             |           | Performance Factor: 1        |
|             |           |     Legal Options: 1,2,3     |
+-------------+-----------+------------------------------+
```

## Set configurations
```
$ xpumcli config -- "Power Limit"=400 -d 0
+-------------+-----------+------------------------------+
| Device Type | Device Id | Configuration                |
+-------------+-----------+------------------------------+
| GPU         | 0         | Power Limit(w): 400          |
|             |           |     Legal Range: 0 to 500    |
|             |           |                              |
|             |           | GPU Frequency(MHz): 300,     |
|             |           |     Legal Range: 100 to 1500 |
|             |           |                              |
|             |           | Memory Frequency(MHz): 800   |
|             |           |     Legal Range: 200 to 1000 |
|             |           |                              |
|             |           | Performance Factor: 1        |
|             |           |     Legal Options: 1,2,3     |
+-------------+-----------+------------------------------+
```

```
$ xpumcli config -- "Power Limit"=400 -g 0
+-------------+-----------+------------------------------+
| Device Type | Device Id | Configuration                |
+-------------+-----------+------------------------------+
| GPU         | 0         | Power Limit(w): 400          |
|             |           |     Legal Range: 0 to 500    |
|             |           |                              |
|             |           | GPU Frequency(MHz): 300,     |
|             |           |     Legal Range: 100 to 1500 |
|             |           |                              |
|             |           | Memory Frequency(MHz): 800   |
|             |           |     Legal Range: 200 to 1000 |
|             |           |                              |
|             |           | Performance Factor: 1        |
|             |           |     Legal Options: 1,2,3     |
|             +-----------+------------------------------+
|             | 1         | Power Limit(w): 400          |
|             |           |     Legal Range: 0 to 500    |
|             |           |                              |
|             |           | GPU Frequency(MHz): 300,     |
|             |           |     Legal Range: 100 to 1500 |
|             |           |                              |
|             |           | Memory Frequency(MHz): 800   |
|             |           |     Legal Range: 200 to 1000 |
|             |           |                              |
|             |           | Performance Factor: 1        |
|             |           |     Legal Options: 1,2,3     |
+-------------+-----------+------------------------------+
```

# FirmwareUpdate subcommand
## Help info
```
$ xpumcli firmwareUpdate --help

firmwareUpdate -- used to update firmware for devices.

Usage: xpumcli firmwareUpdate [-h] [-d] [-g] [-l] [-u]

Optional arguments:
    -h, --help              Show this help message and exit
    -d <deviceId>, --device <deviceId>      
                            The device id to query
    -g <groupId>, --group <groupId>         
                            The group id to query
    -l, --list              List firmware info
    -u, --update            Update firmware
        -T <firmware name>, --Type <firmware name>      
            Firmware name to update
        -L <image path>, --local <image path>           
            Local image path, used when firmware iamge is located in server 
                local path.
        -I <image uri>, --Image <image uri>             
            URI referencing a software image to be retrieved
        -U <username>, --Username <username>            
            Username to access <image uri>
        -P <password>, --Password <passowrd>            
            Password to access <image uri>
        -R, --Restart                                   
            Restart server when update finish
```
## Display update status

$ xpumcli firmwareupdate -d 0 -l
```
// no update task running
+-------------+-----------+--------------------------+
| Device Type | Device Id | Firmware Info            |
+-------------+-----------+--------------------------+
| GPU         | 0         | Firmware Name: GSC       |
|             |           | Version: ATS1_0.7178     |
|             |           +--------------------------+
|             |           | Firmware Name: Firmware2 |
|             |           | Version: 1.00            |
+-------------+-----------+--------------------------+

// during updating
+-------------+-----------+-------------------------------------------+
| Device Type | Device Id | Firmware Info                             |
+-------------+-----------+-------------------------------------------+
| GPU         | 0         | Firmware Name: GSC                        |
|             |           | Version: ATS1_0.7178                      |
|             |           |                                           |
|             |           | Update Running:                           |
|             |           | File Name: image.0                        |
|             |           | Image URI: http://localhost/image/image.0 |
|             |           | Restart Server When Finished: true,       |
|             |           | Task Start Time: 2021-07-09 16:03:36      |
|             |           +-------------------------------------------+
|             |           | Firmware Name: Firmware2                  |
|             |           | Version: 1.00                             |
+-------------+-----------+-------------------------------------------+
```

## Update firmware

```
$ xpumcli firmwareupdate -d 0 -u -T GSC  -I "http://localhost/image/image.0" -R
+-------------+-----------+-------------------------------------------+
| Device Type | Device Id | Firmware Info                             |
+-------------+-----------+-------------------------------------------+
| GPU         | 0         | Firmware Name: GSC                        |
|             |           | Version: ATS1_0.7178                      |
|             |           |                                           |
|             |           | Update Running:                           |
|             |           | File Name: image.0                        |
|             |           | Image URI: http://localhost/image/image.0 |
|             |           | Restart Server When Finished: true,       |
|             |           | Task Start Time: 2021-07-09 16:03:36      |
|             |           +-------------------------------------------+
|             |           | Firmware Name: Firmware2                  |
|             |           | Version: 1.00                             |
+-------------+-----------+-------------------------------------------+

$ xpumcli firmwareupdate -d 0 -u -T Firmware2  -L "/tmp/firmware.1"
+-------------+-----------+-------------------------------------------+
| Device Type | Device Id | Firmware Info                             |
+-------------+-----------+-------------------------------------------+
| GPU         | 0         | Firmware Name: GSC                        |
|             |           | Version: ATS1_0.7178                      |
|             |           |                                           |
|             |           | Update Running:                           |
|             |           | File Name: image.0                        |
|             |           | Image URI: http://localhost/image/image.0 |
|             |           | Restart Server When Finished: true,       |
|             |           | Task Start Time: 2021-07-09 16:03:36      |
|             |           +-------------------------------------------+
|             |           | Firmware Name: Firmware2                  |
|             |           | Version: 1.00                             |
|             |           |                                           |
|             |           | Update Running:                           |
|             |           | File Name: firmware.1                     |
|             |           | Image Path: /tmp/firmware.1               |
|             |           | Restart Server When Finished: false,      |
|             |           | Task Start Time: 2021-07-09 16:03:36      |
+-------------+-----------+-------------------------------------------+
```

```
// Mean while run another firmware update task

$ xpumcli firmwareupdate -d 0 -u -T GSC -I "http://localhost/image/image.0" -R
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

Usage: xpumcli stats [-h] [-g] [-d]

Optional arguments:
    -h, --help              Show this help message and exit
    -d <deviceId>, --device <deviceId>  
                            The device id to query
    -g <groupId>, --group <groupId>     
                            The group id to query
    --tile <tileId>
```
## List stats
```
$ xpumcli stats
Device Type: GPU
Device Id: 0
Tile: 0
+--------------------------------+-----------------------------------------+
| Device Type                    | GPU                                     |
+--------------------------------+-----------------------------------------+
| Device Id                      | 0                                       |
+--------------------------------+-----------------------------------------+
| Start Time                     |2021-07-09 16:03:36                      |
| End Time                       |2021-07-09 19:03:36                      |
| Energy Consumed (J)            |2000                                     |
+--------------------------------+-----------------------------------------+
| GPU_Computation (%)            |avg: 70                                  |
|                                |min: 70                                  |
|                                |max: 70                                  |
+--------------------------------+-----------------------------------------+
| Occupation (%)                 |avg: 70                                  |
|                                |min: 70                                  |
|                                |max: 70                                  |
+--------------------------------+-----------------------------------------+
| GPU_Frequency (MHz)            |avg: 500                                 |
|                                |min: 300                                 |
|                                |max: 700                                 |
+--------------------------------+-----------------------------------------+
| Device Id                      | 1                                       |
+--------------------------------+-----------------------------------------+
| Start Time                     |2021-07-09 16:03:36                      |
| End Time                       |2021-07-09 19:03:36                      |
| Energy Consumed (J)            |2000                                     |
+--------------------------------+-----------------------------------------+
| GPU_Computation (%)            |avg: 70                                  |
|                                |min: 70                                  |
|                                |max: 70                                  |
+--------------------------------+-----------------------------------------+
| Occupation (%)                 |avg: 70                                  |
|                                |min: 70                                  |
|                                |max: 70                                  |
+--------------------------------+-----------------------------------------+
| GPU_Frequency (MHz)            |avg: 500                                 |
|                                |min: 300                                 |
|                                |max: 700                                 |
+--------------------------------+-----------------------------------------+
```