# Authentication

The XPU Manager agent's authenticate method can be configured through configuration file.

Default is no auth, other option is:

- basic auth

The configuration file can be modified through provisioning way, like ansible.


# URI

## XPUM Version Info

/rest/v1/version

## Device Info

/rest/v1/devices

/rest/v1/devices/{deviceId}

## Groups Management

/rest/v1/groups

/rest/v1/groups/{groupId}

## Agent setting

/rest/v1/globalSettings

## Telemetry

/rest/v1/devices/{deviceId}/telemetry

/rest/v1/devices/{deviceId}/telemetry/memory

/rest/v1/devices/{deviceId}/telemetry/power

/rest/v1/devices/{deviceId}/telemetry/temperature

/rest/v1/devices/{deviceId}/telemetry/fabricPort

/rest/v1/devices/{deviceId}/telemetry/frequency

## Group Telemetry

/rest/v1/groups/{groupId}/telemetry

/rest/v1/groups/{groupId}/telemetry/memory

/rest/v1/groups/{groupId}/telemetry/power

/rest/v1/groups/{groupId}/telemetry/temperature

/rest/v1/groups/{groupId}/telemetry/fabricPort

/rest/v1/groups/{groupId}/telemetry/frequency

## Health

/rest/v1/devices/{deviceId}/health

/rest/v1/devices/{deviceId}/health/memory

/rest/v1/devices/{deviceId}/health/temperature

/rest/v1/devices/{deviceId}/health/fabricPort

## Group Health

/rest/v1/groups/{groupId}/health

/rest/v1/groups/{groupId}/health/memory

/rest/v1/groups/{groupId}/health/temperature

/rest/v1/groups/{groupId}/health/fabricPort

## Historical Events

/rest/v1/events

## Configuration

/rest/v1/devices/{deviceId}/configuration 

## Group Configuration

/rest/v1/groups/{groupId}/configuration 

## Firmware Update

/rest/v1/devices/{deviceId}/firmware

## Group Firmware Update

/rest/v1/groups/{groupId}/firmware

## Diagnostics

/rest/v1/devices/{deviceId}/diagnostics

## Group Diagnostics

/rest/v1/groups/{groupId}/diagnostics

# APIs

## XPUM version info

### Get xpum version info

Get versions information about XPU Manager

#### Request URL

/rest/v1/version

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/version

#### Response field description

| Field            | Type   | Description                              |
| ---------------- | ------ | ---------------------------------------- |
| Version          | String | Version of XPUM service                  |
| BuildId          | String | Build id of XPUM service                 |
| LevelZeroVersion | String | Version of underlying level-zero library |

#### Response example
```json
{
    "Version": "1.0.0",
    "BuildId": "10",
    "LevelZeroVersion": "1.2.13"
}
```

## Device info

### Get all devices

Get fundamental info for all devices

#### Request URL
/rest/v1/devices

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/devices

#### Response field description

| Field         | Type   | Description                                                  |
| ------------- | ------ | ------------------------------------------------------------ |
| DeviceId      | Number | Device id of this GPU (may change when device un-plug and re-plug) |
| Properties    | List   | Properties of this GPU                                       |
| DeviceType    | String | Now is just GPU                                              |
| UUID          | String | A unique device identifier                                   |
| DeviceName    | String |                                                              |
| PCIDeviceId   | String |                                                              |
| SubDeviceId   | String |                                                              |
| PCIBDFAddress | String |                                                              |
| VendorName    | String |                                                              |

#### Response example
```json
[
    {
        "DeviceId": 0,
        "DeviceName": "Intel(R) Iris(R) Xe MAX Graphics [0x4905]",
        "DeviceType": 0,
        "PCIBDFAddress": "0000:1a:0.0",
        "PCIDeviceId": "0x4905",
        "SubDeviceId": "0x0",
        "UUID": "00000000-0000-0000-0000-490500008086",
        "VendorName": "Intel(R) Corporation"
    },
    {
        "DeviceId": 1,
        "DeviceName": "Intel(R) Iris(R) Xe MAX Graphics [0x4905]",
        "DeviceType": 0,
        "PCIBDFAddress": "0000:b1:0.0",
        "PCIDeviceId": "0x4905",
        "SubDeviceId": "0x0",
        "UUID": "00000000-0000-0001-0000-490500008086",
        "VendorName": "Intel(R) Corporation"
    }
]
```


### Get specific device info

Detailed info for specific device

#### Request URL
/rest/v1/devices/{deviceId}

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/devices/0

#### Response field description

| Field                     | Type   | Description                                                  |
| ------------------------- | ------ | ------------------------------------------------------------ |
| DeviceId                  | Number | Device id of this GPU (may change when device un-plug and re-plug) |
| Properties                | List   | Properties of this GPU                                       |
| DeviceType                | String | Now is just GPU                                              |
| UUID                      | String | A unique device identifier                                   |
| DeviceName                | String |                                                              |
| PCIDeviceId               | String |                                                              |
| SubDeviceId               | String |                                                              |
| PCIBDFAddress             | String |                                                              |
| SerialNumber              | String |                                                              |
| VendorName                | String |                                                              |
| BoardNumber               | String |                                                              |
| BrandName                 | String |                                                              |
| DriverVersion             | String |                                                              |
| FirmwareName              | String |                                                              |
| FirmwareVersion           | String |                                                              |
| NumberofSubDevices        | String |                                                              |
| NumberofSlices            | String |                                                              |
| NumberofSubSlicesPerSlice | String |                                                              |
| NumberofEUsPerSubSlice    | String |                                                              |
| NumberofThreadsPerEU      | String |                                                              |
| CoreClockRate             | String |                                                              |
| MaxMemAllocSize           | String |                                                              |
| MaxHardwareContexts       | String |                                                              |
| MaxCommandQueuePriority   | String |                                                              |
| PysicalEUSIMDWidth        | String |                                                              |
| TimerResolution           | String |                                                              |
| TimestampValidBits        | String |                                                              |
| PCIVendorId               | String |                                                              |
| KernelTimestampValidBits  | String |                                                              |
| Flags                     | String |                                                              |
| MemoryPhysicalSize        | String |                                                              |
| MemoryFreeSize            | String |                                                              |

#### Response example
```json
{
    "BoardNumber": "unknown",
    "BrandName": "Intel(R) Corporation",
    "CoreClockRate": "1650 MHz",
    "DeviceId": 0,
    "DeviceName": "Intel(R) Iris(R) Xe MAX Graphics [0x4905]",
    "DeviceType": "GPU",
    "DriverVersion": "16863095",
    "Flags": "0",
    "KernelTimestampValidBits": "32",
    "MaxCommandQueuePriority": "0",
    "MaxHardwareContexts": "65536",
    "MaxMemAllocSize": "1609.60 MiB",
    "MemoryFreeSize": "4005.75 MiB",
    "MemoryHealth": "All memory channels are healthy.",
    "MemoryPhysicalSize": "4024.00 MiB",
    "NumberOfEusPerSubSlice": "16",
    "NumberOfSlices": "1",
    "NumberOfSubDevices": "0",
    "NumberOfSubSlicesPerSubSlice": "6",
    "NumberOfThreadsPerEu": "7",
    "PCIBdfAddress": "0000:1a:0.0",
    "PCIDeviceId": "0x4905",
    "PCIVendorId": "0x8086",
    "PysicalEuSimdWidth": "8",
    "SerialNumber": "unknown",
    "SubDeviceId": "0x0",
    "TimerResolution": "52",
    "TimestampValidBits": "36",
    "UUID": "00000000-0000-0000-0000-490500008086",
    "VendorName": "Intel(R) Corporation"
}
```

## Groups Management

### Create group

Create device group

#### Request URL
/rest/v1/groups

#### HTTP request method
POST

#### Request field description
| Field     | Type   | R/O      | Description |
| --------- | ------ | -------- | ----------- |
| GroupName | String | Required | Group name  |

#### Request example
http://localhost:5000/rest/v1/groups

```json
{
    "GroupName": "All Gpus"
}
```

#### Response field description
| Field     | Type   | Description                           |
| --------- | ------ | ------------------------------------- |
| GroupName | String | Group name                            |
| GroupId   | Number | Group id                              |
| DeviceIds | List   | Device ids that belongs to this group |

#### Response example
```json
{
    "GroupName": "All Gpus",
    "GroupId": 0,
    "DeviceIds": []
}
```

### Add device to group

Add device to group

#### Request URL
/rest/v1/groups/{groupId}

#### HTTP request method
POST

#### Request field description
| Field    | Type   | R/O      | Description                                        |
| -------- | ------ | -------- | -------------------------------------------------- |
| DeviceIdToAdd | Number | Optional | Device id for the device to be added to this group |
| DeviceIdToRemove | Number | Optional | Device id for the device to be removed from this group |

#### Request example
http://localhost:5000/rest/v1/groups/0

```json
{
    "DeviceIdToAdd": 0
}
```

#### Response field description
| Field     | Type   | Description                           |
| --------- | ------ | ------------------------------------- |
| GroupName | String | Group name                            |
| GroupId   | Number | Group id                              |
| DeviceIds | List   | Device ids that belongs to this group |

#### Response example
```json
{
    "GroupName": "All Gpus",
    "GroupId": 0,
    "DeviceIds": [
        0
    ]
}
```

### Get group info

Get group info

#### Request URL
/rest/v1/groups/{groupId}

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/groups/0

#### Response field description
| Field     | Type   | Description                           |
| --------- | ------ | ------------------------------------- |
| GroupName | String | Group name                            |
| GroupId   | Number | Group id                              |
| DeviceIds | List   | Device ids that belongs to this group |

#### Response example
```json
{
    "GroupName": "All Gpus",
    "GroupId": 0,
    "DeviceIds": []
}
```

### Remove a group

Remove a group

#### Request URL
/rest/v1/groups/{groupId}

#### HTTP request method
DELETE

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/groups/0

#### Response field description
N/A

#### Response example
N/A


## Agent setting

### Set agent properties
Set agent properties.

#### Request URL
/rest/v1/globalSettings

#### HTTP request method
POST

#### Request field description
| Field             | Type   | R/O      | Description                                                  |
| ----------------- | ------ | -------- | ------------------------------------------------------------ |
| EventLimit        | Number | Optional | The limit count of latest historical event that will be kept by agent |
| TelemetryInterval | Number | Optional | The time interval(in milliseoncds) by which xpum daemon tries to get data. Limits 100 to 1000, default 1000. |
| AggregatePeriod   | Number | Optional | The time period(in seconds) by which to aggregate statistics like min, max, average values. Limits 5 to 600, default 60 |

#### Request example
http://localhost:5000/rest/v1/globalSettings

```json
{
    "EventLimit": 1000
}
```

#### Response field description
| Field             | Type   | Description                                                  |
| ----------------- | ------ | ------------------------------------------------------------ |
| EventLimit        | Number | The limit count of latest historical event that will be kept by agent |
| TelemetryInterval | Number | The time interval(in milliseconds) by which xpum daemon tries to get data. Limits 100 to 1000, default 1000. |
| AggregatePeriod   | Number | The time period(in seconds) by which to aggregate statistics like min, max, average values. Limits 5 to 600, default 60 |

#### Response example
```json
{
    "EventLimit": 1000,
    "TelemetryInterval": 1000,
    "AggregatePeriod": 60
}
```

### Get agent settings

Get agent settings.

#### Request URL
/rest/v1/globalSettings

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/globalSettings

#### Response field description
| Field             | Type   | Description                                                  |
| ----------------- | ------ | ------------------------------------------------------------ |
| EventLimit        | Number | The limit count of latest historical event that will be kept by agent, default is 100 |
| TelemetryInterval | Number | The time interval(in milliseconds) by which xpum daemon tries to get data. Limits 100 to 1000, default 1000. |
| AggregatePeriod   | Number | Set the time period(in seconds) by which to aggregate statistics like min, max, average values. Limits 5 to 600, default 60 |

#### Response example
```json
{
    "EventLimit": 1000,
    "TelemetryInterval": 1000,
    "AggregatePeriod": 60
}
```

## Telemetry 

### Get telemetry data

Get telemetry data from a device, like used memory, power, temperature, fabric port speed, frequency, engine utilization

#### Request URL
/rest/v1/devices/{deviceId}/telemetry

#### HTTP request method
GET

#### Request field description
| Field    | Type    | R/O      | Description                                                  |
| -------- | ------- | -------- | ------------------------------------------------------------ |
| Realtime | Boolean | Optional | When set true, get telemetry in real time; otherwise use latest queried data. |
| Dump     | Boolean | Optional | When set true,  will return raw data of one aggregate period |


#### Request example
http://localhost:5000/rest/v1/devices/0/telemetry

http://localhost:5000/rest/v1/devices/0/telemetry?Realtime=true

http://localhost:5000/rest/v1/devices/0/telemetry?Dump=true

#### Response field description

| Field             | Type      | Description                                                  |
| ----------------- | --------- | ------------------------------------------------------------ |
| DeviceId          | Number    | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType        | String    | Now is just GPU                                              |
| UUID              | String    | A unique device identifier                                   |
| Timestamp         | Timestamp | The time when the data retrieved                             |
| Dump              | List      | Dumped data                                                  |
| current           | Number    | Current value of telemetered data                            |
| avg               | Number    | Average value of telemetered data                            |
| max               | Number    | Maximum value of telemetered data                            |
| min               | Number    | Minimum value of telemetered data                            |
| Power             | Object    | Telemetered power data of this device, unit is Watts(W)      |
| Frequency         | Object    | Telemetered frequency data of this device, unit is MHz       |
| Temperature       | Object    | Telemetered thermal data of this device, unit is Celsius degree |
| MemoryUsed        | Object    | Telemetered memory used data of this device, unit is MiB     |
| EngineUtilization | Object    | Telemetered engine utilization data of this device, unit is percent(%) |
| FabricPortSpeed   | Object    | Telemetered fabric port speed data of this device, unit is bits per second |

#### Response example
```json
// no real time
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "Power": {
        "current": 71,
        "avg": 71,
        "max": 71,
        "min": 71
    },
    "Frequency": {
        "current": 300,
        "avg": 300,
        "max": 300,
        "min": 300
    },
    "Temperature": {
        "current": 35,
        "avg": 36,
        "max": 36,
        "min": 35
    },
    "Memory Used": {
        "current": 32.80,
        "avg": 32.80,
        "max": 32.81,
        "min": 32.80
    },
    "EngineUtilization": {
        "current": 0,
        "avg": 0,
        "max": 0,
        "min": 0
    },
    "FabricPortSpeed": {
        "current": 0,
        "avg": 0,
        "max": 0,
        "min": 0
    }
}
// real time
{
    "DeviceId": 0,
    "Device Type": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "Power": {
        "current": 71
    },
    "Frequency": {
        "current": 300
    },
    "Temperature": {
        "current": 35
    },
    "MemoryUsed": {
        "current": 32.80
    },
    "EngineUtilization": {
        "current": 0
    },
    "FabricPortSpeed": {
        "current": 0
    }
}

// dump data
{
    "Device Id": 0,
    "Device Type": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "Dump": [
        {
            "Timestamp": "2021-07-09 16:03:36",
            "Power": {
                "current": 71
            },
            "Frequency": {
                "current": 300
            },
            "Temperature": {
                "current": 35
            },
            "Memory Used": {
                "current": 32.80
            },
            "EngineUtilization": {
                "current": 0
            },
            "FabricPortSpeed": {
                "current": 0
            }
        },
        {
            "Timestamp": "2021-07-09 16:03:37",
            "Power": {
                "current": 71
            },
            "Frequency": {
                "current": 300
            },
            "Temperature": {
                "current": 35
            },
            "MemoryUsed": {
                "current": 32.80
            },
            "EngineUtilization": {
                "current": 0
            },
            "FabricPortSpeed": {
                "current": 0
            }
        },
        {
            "Timestamp": "2021-07-09 16:03:38",
            "Power": {
                "current": 71
            },
            "Frequency": {
                "current": 300
            },
            "Temperature": {
                "current": 35
            },
            "MemoryUsed": {
                "current": 32.80
            },
            "EngineUtilization": {
                "current": 0
            },
            "FabricPortSpeed": {
                "current": 0
            }
        }
    ]
}
```
### Get memory telemetry data

Get memory telemetry data from a device; Other component is the same.

#### Request URL
/rest/v1/devices/{deviceId}/telemetry/memory

#### HTTP request method
GET

#### Request field description
| Field    | Type    | R/O      | Description                                                  |
| -------- | ------- | -------- | ------------------------------------------------------------ |
| Realtime | Boolean | Optional | When set true, get telemetry in real time; otherwise use latest queried data. |
| Dump     | Boolean | Optional | When set true,  will return raw data of one aggregate period |


#### Request example
http://localhost:5000/rest/v1/devices/0/telemetry/memory

http://localhost:5000/rest/v1/devices/0/telemetry/memory?Realtime=true

http://localhost:5000/rest/v1/devices/0/telemetry/memory?Dump=true

#### Response field description

| Field      | Type      | Description                                                  |
| ---------- | --------- | ------------------------------------------------------------ |
| DeviceId   | Number    | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType | String    | Now is just GPU                                              |
| UUID       | String    | A unique device identifier                                   |
| Timestamp  | Timestamp | The time when the data retrieved                             |
| Dump       | List      | Dumped data                                                  |
| current    | Number    | Current value of telemetered data                            |
| avg        | Number    | Average value of telemetered data                            |
| max        | Number    | Maximum value of telemetered data                            |
| min        | Number    | Minimum value of telemetered data                            |
| MemoryUsed | Object    | Telemetered memory used data of this device, unit is MiB     |

#### Response example
```json
// no real time
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "MemoryUsed": {
        "current": 32.80,
        "avg": 32.80,
        "max": 32.81,
        "min": 32.80
    }
}
// real time
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "MemoryUsed": {
        "current": 32.80
    }
}

// dump data
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "Dump": [
        {
            "Timestamp": "2021-07-09 16:03:36",
            "MemoryUsed": {
                "current": 32.80
            }
        },
        {
            "Timestamp": "2021-07-09 16:03:37",
            "MemoryUsed": {
                "current": 32.80
            }
        },
        {
            "Timestamp": "2021-07-09 16:03:38",
            "MemoryUsed": {
                "current": 32.80
            }
        }
    ]
}
```

## Telemetry by group

### Get group telemetry data

Get telemetry data from a group, like used memory, power, temperature, fabric port speed, frequency, engine utilization

#### Request URL
/rest/v1/groups/{groupId}/telemetry

#### HTTP request method
GET

#### Request field description
| Field    | Type    | R/O      | Description                                                  |
| -------- | ------- | -------- | ------------------------------------------------------------ |
| Realtime | Boolean | Optional | When set true, get telemetry in real time; otherwise use latest queried data. |
| Dump     | Boolean | Optional | When set true,  will return raw data of one aggregate period |


#### Request example
http://localhost:5000/rest/v1/groups/0/telemetry

http://localhost:5000/rest/v1/groups/0/telemetry?Realtime=true

http://localhost:5000/rest/v1/groups/0/telemetry?Dump=true

#### Response field description

| Field             | Type   | Description                                                  |
| ----------------- | ------ | ------------------------------------------------------------ |
| GroupId           | Number | Group Id                                                     |
| GroupName         | String | Group Name                                                   |
| Telemetries       | List   | Telemetries of devices                                       |
| DeviceId          | Number | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType        | String | Now is just GPU                                              |
| UUID              | String | A unique device identifier                                   |
| Dump              | List   | Dumped data                                                  |
| current           | Number | Current value of telemetered data                            |
| avg               | Number | Average value of telemetered data                            |
| max               | Number | Maximum value of telemetered data                            |
| min               | Number | Minimum value of telemetered data                            |
| Power             | Object | Telemetered power data of this device, unit is Watts(W)      |
| Frequency         | Object | Telemetered frequency data of this device, unit is MHz       |
| Temperature       | Object | Telemetered thermal data of this device, unit is Celsius degree |
| MemoryUsed        | Object | Telemetered memory used data of this device, unit is MiB     |
| EngineUtilization | Object | Telemetered engine utilization data of this device, unit is percent(%) |
| FabricPortSpeed   | Object | Telemetered fabric port speed data of this device, unit is bits per second |

#### Response example
```json
// no real time
{
    "GroupId": "0",
    "GroupName": "All Gpus",
    "Telemetries": [
        {
            "DeviceId": 0,
            "DeviceType": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "Power": {
                "current": 71,
                "avg": 71,
                "max": 71,
                "min": 71
            },
            "Frequency": {
                "current": 300,
                "avg": 300,
                "max": 300,
                "min": 300
            },
            "Temperature": {
                "current": 35,
                "avg": 36,
                "max": 36,
                "min": 35
            },
            "Memory Used": {
                "current": 32.80,
                "avg": 32.80,
                "max": 32.81,
                "min": 32.80
            },
            "Engine Utilization": {
                "current": 0,
                "avg": 0,
                "max": 0,
                "min": 0
            },
            "Fabric Port Speed": {
                "current": 0,
                "avg": 0,
                "max": 0,
                "min": 0
            }
        }
    ]
}
// real time
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Telemetries": [
        {
            "Device Id": "0",
            "Device Type": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "Power": {
                "current": 71
            },
            "Frequency": {
                "current": 300
            },
            "Temperature": {
                "current": 35
            },
            "Memory Used": {
                "current": 32.80
            },
            "Engine Utilization": {
                "current": 0
            },
            "Fabric Port Speed": {
                "current": 0
            }
        }
    ]
}

// Dump data
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Telemetries": [
        {
            "Device Id": "0",
            "Device Type": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "Dump": [
                {
                    "Timestamp": "2021-07-09 16:03:36",
                    "Power": {
                        "current": 71
                    },
                    "Frequency": {
                        "current": 300
                    },
                    "Temperature": {
                        "current": 35
                    },
                    "Memory Used": {
                        "current": 32.80
                    },
                    "EngineUtilization": {
                        "current": 0
                    },
                    "FabricPortSpeed": {
                        "current": 0
                    }
                },
                {
                    "Timestamp": "2021-07-09 16:03:37",
                    "Power": {
                        "current": 71
                    },
                    "Frequency": {
                        "current": 300
                    },
                    "Temperature": {
                        "current": 35
                    },
                    "MemoryUsed": {
                        "current": 32.80
                    },
                    "EngineUtilization": {
                        "current": 0
                    },
                    "FabricPortSpeed": {
                        "current": 0
                    }
                },
                {
                    "Timestamp": "2021-07-09 16:03:38",
                    "Power": {
                        "current": 71
                    },
                    "Frequency": {
                        "current": 300
                    },
                    "Temperature": {
                        "current": 35
                    },
                    "MemoryUsed": {
                        "current": 32.80
                    },
                    "EngineUtilization": {
                        "current": 0
                    },
                    "FabricPortSpeed": {
                        "current": 0
                    }
                }
            ]
        }
    ]
}
```
### Get group memory telemetry data

Get memory telemetry data from a device; Other component is the same.

#### Request URL
/rest/v1/groups/{groupId}/telemetry/memory

#### HTTP request method
GET

#### Request field description
| Field    | Type    | R/O      | Description                                                  |
| -------- | ------- | -------- | ------------------------------------------------------------ |
| Realtime | Boolean | Optional | When set true, get telemetry in real time; otherwise use latest queried data. |
| Dump     | Boolean | Optional | When set true,  will return raw data of one aggregate period |


#### Request example
http://localhost:5000/rest/v1/groups/0/telemetry/memory

http://localhost:5000/rest/v1/groups/0/telemetry/memory?Realtime=true

http://localhost:5000/rest/v1/groups/0/telemetry/memory?Dump=true

#### Response field description

| Field       | Type   | Description                                                  |
| ----------- | ------ | ------------------------------------------------------------ |
| GroupId     | Number | Group Id                                                     |
| GroupName   | String | Group Name                                                   |
| Telemetries | List   | Telemetries of devices                                       |
| DeviceId    | Number | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType  | String | Now is just GPU                                              |
| UUID        | String | A unique device identifier                                   |
| current     | Number | Current value of telemetered data                            |
| avg         | Number | Average value of telemetered data                            |
| max         | Number | Maximum value of telemetered data                            |
| min         | Number | Minimum value of telemetered data                            |
| MemoryUsed  | Object | Telemetered memory used data of this device, unit is MiB     |

#### Response example
```json
// no real time
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Telemetries": [
        {
            "DeviceId": 0,
            "DeviceType": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "MemoryUsed": {
                "current": 32.80,
                "avg": 32.80,
                "max": 32.81,
                "min": 32.80
            }
        }
    ]
}
// real time
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Telemetries": [
        {
            "DeviceId": 0,
            "DeviceType": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "MemoryUsed": {
                "current": 32.80
            }
        }
    ]
}

// Dump data
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Telemetries": [
        {
            "DeviceId": 0,
            "DeviceType": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "Dump": [
                {
                    "Timestamp": "2021-07-09 16:03:36",
                    "MemoryUsed": {
                        "current": 32.80
                    }
                },
                {
                    "Timestamp": "2021-07-09 16:03:37",
                    "MemoryUsed": {
                        "current": 32.80
                    }
                },
                {
                    "Timestamp": "2021-07-09 16:03:38",
                    "MemoryUsed": {
                        "current": 32.80
                    }
                }
            ]
        }
    ]
}
```


## Health

### Get health status from a device

Get health status from a device for different components like memory, temperature, fabric port.

#### Request URL
/rest/v1/devices/{deviceId}/health

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/devices/0/health

#### Response field description

| Field       | Type   | Description                                                  |
| ----------- | ------ | ------------------------------------------------------------ |
| DeviceId    | Number | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType  | String | Now is just GPU                                              |
| UUID        | String | A unique device identifier                                   |
| Status      | String | OK;Warning;Error                                             |
| Description | String |                                                              |
| Threshold   | Number | Temperature threshold for health alert, in Celsius degree (ThresholdCelsius?) |
| Temperature | Object | Temperature health status                                    |
| Memory      | Object | Memory health status                                         |
| FabricPort  | Object | Fabric Port health status                                    |

#### Response example
```json
{
    "Device Id": "0",
    "Device Type": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "Temperature": {
        "Status": "OK",
        "Description": "All temperatures are OK.",
        "Threshold": 50
    },
    "Memory": {
        "Status": "OK",
        "Description": "All memory channels are OK."
    },
    "FabricPort": {
        "Status": "OK",
        "Description": "All fabric ports are OK."
    }
}
```

### Get temperature health status

Get temperature health status from a device.

#### Request URL
/rest/v1/devices/{deviceId}/health/temperature

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/devices/0/health/temperature

#### Response field description

| Field       | Type   | Description                                                  |
| ----------- | ------ | ------------------------------------------------------------ |
| DeviceId    | Number | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType  | String | Now is just GPU                                              |
| UUID        | String | A unique device identifier                                   |
| Status      | String | OK;Warning;Critical                                          |
| Description | String |                                                              |
| Threshold   | Number | Temperature threshold for health alert, in Celsius degree (ThresholdCelsius?) |
| Temperature | Object | Temperature health status                                    |

#### Response example
```json
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "Temperature": {
        "Status": "OK",
        "Description": "All temperatures are OK.",
        "Threshold": 50
    }
}
```

### Set temperature threshold for device.

Set temperature threshold.

#### Request URL
/rest/v1/devices/{deviceId}/health/temperature

#### HTTP request method
PUT

#### Request field description
| Field     | Type   | R/O      | Description                                               |
| --------- | ------ | -------- | --------------------------------------------------------- |
| Threshold | Number | Required | Temperature threshold for health alert, in Celsius degree |

#### Request example
http://localhost:5000/rest/v1/devices/0/health/temperature

```json
{
    "Threshold":50
}
```

#### Response field description

| Field       | Type   | Description                                                  |
| ----------- | ------ | ------------------------------------------------------------ |
| DeviceId    | Number | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType  | String | Now is just GPU                                              |
| UUID        | String | A unique device identifier                                   |
| Status      | String | OK;Warning;Critical                                          |
| Description | String |                                                              |
| Threshold   | Number | Temperature threshold for health alert, in Celsius degree    |
| Temperature | Object | Temperature health status                                    |

#### Response example
```json
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "Temperature": {
        "Status": "OK",
        "Description": "All temperatures are OK.",
        "Threshold": 50
    }
}
```

## Health by group

### Get health status from a group

Get health status from a group for different components like memory, temperature, fabric port.

#### Request URL
/rest/v1/groups/{groupId}/health

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/groups/0/health

#### Response field description

| Field       | Type   | Description                                                  |
| ----------- | ------ | ------------------------------------------------------------ |
| GroupId     | Number | Group Id                                                     |
| GroupName   | String | Group Name                                                   |
| Health      | List   | Health for different devices                                 |
| DeviceId    | Number | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType  | String | Now is just GPU                                              |
| UUID        | String | A unique device identifier                                   |
| Status      | String | OK;Warning;Error                                             |
| Description | String |                                                              |
| Threshold   | Number | Temperature threshold for health alert, in Celsius degree    |
| Temperature | Object | Temperature health status                                    |
| Memory      | Object | Memory health status                                         |
| FabricPort  | Object | Fabric Port health status                                    |

#### Response example
```json
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Health": [
        {
            "Device Id": "0",
            "Device Type": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "Temperature": {
                "Status": "OK",
                "Description": "All temperatures are OK.",
                "Threshold": 50
            },
            "Memory": {
                "Status": "OK",
                "Description": "All memory channels are OK."
            },
            "FabricPort": {
                "Status": "OK",
                "Description": "All fabric ports are OK."
            }
        }
    ]
}
```

### Get group temperature health status

Get temperature health status from a group.

#### Request URL
/rest/v1/groups/{groupId}/health/temperature

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/groups/0/health/temperature

#### Response field description

| Field       | Type   | Description                                                  |
| ----------- | ------ | ------------------------------------------------------------ |
| GroupId     | Number | Group Id                                                     |
| GroupName   | String | Group Name                                                   |
| Health      | List   | Health for different devices                                 |
| DeviceId    | Number | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType  | String | Now is just GPU                                              |
| UUID        | String | A unique device identifier                                   |
| Status      | String | OK;Warning;Critical                                          |
| Description | String |                                                              |
| Threshold   | Number | Temperature threshold for health alert, in Celsius degree    |
| Temperature | Object | Temperature health status                                    |

#### Response example
```json
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Health": [
        {
            "DeviceId": 0,
            "DeviceType": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "Temperature": {
                "Status": "OK",
                "Description": "All temperatures are OK.",
                "Threshold": 50
            }
        }
    ]
}
```

### Set temperature threshold for group.

Set temperature threshold.

#### Request URL
/rest/v1/groups/{groupId}/health/temperature

#### HTTP request method
PUT

#### Request field description
| Field     | Type   | R/O      | Description                                               |
| --------- | ------ | -------- | --------------------------------------------------------- |
| Threshold | Number | Required | Temperature threshold for health alert, in Celsius degree |

#### Request example
http://localhost:5000/rest/v1/groups/0/health/temperature

```json
{
    "Threshold":50
}
```

#### Response field description

| Field       | Type   | Description                                                  |
| ----------- | ------ | ------------------------------------------------------------ |
| GroupId     | Number | Group Id                                                     |
| GroupName   | String | Group Name                                                   |
| Health      | List   | Health for different devices                                 |
| DeviceId    | Number | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType  | String | Now is just GPU                                              |
| UUID        | String | A unique device identifier                                   |
| Status      | String | OK;Warning;Critical                                          |
| Description | String |                                                              |
| Threshold   | Number | Temperature threshold for health alert, in Celsius degree    |
| Temperature | Object | Temperature health status                                    |

#### Response example
```json
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Health": [
        {
            "DeviceId": 0,
            "DeviceType": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "Temperature": {
                "Status": "OK",
                "Description": "All temperatures are OK.",
                "Threshold": 50
            }
        }
    ]
}
```


## Historical Events 

### Get events for a device

Get historical events for a device. XPU Manager keeps latest 100 records.

#### Request URL
/rest/v1/events

#### HTTP request method
GET

#### Request field description
| Field                    | Type      | R/O      | Description                  |
| ------------------------ | --------- | -------- | ---------------------------- |
| BeginTime                | Timestamp | Optional |                              |
| EndTime                  | Timestamp | Optional |                              |
| Severity                 | String    | Optional | Filter events by severity    |
| Component                | String    | Optional | Filter events by component   |
| EventType                | String    | Optional | Filter events by event type  |
| DeviceType               | String    | Optional | Filter events by device type |
| DeviceId                 | Number    | Optional | Filter events by device id   |
| Pagination parameters... |           |          |                              |
| Sort parameters...       |           |          |                              |

#### Request example
http://localhost:5000/rest/v1/events

#### Response field description

| Field          | Type      | Description                                                  |
| -------------- | --------- | ------------------------------------------------------------ |
| DeviceId       | Number    | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType     | String    | Now is just GPU                                              |
| UUID           | String    | A unique device identifier                                   |
| Events         | List      |                                                              |
| EventId        | Number    | Event id                                                     |
| Severity       | String    | Event severity, Normal\|Warning\|Critical                    |
| Message        | String    | Event message                                                |
| EventTimestamp | Timestamp | The time the event occurred                                  |
| Component      | String    | Component that generates this event                          |
| EventType      | String    | The event type                                               |

#### Response example
```json
[
    {
        "EventId": 0,
        "Severity": "Critical",
        "Message": "GPU temperature is 81 Celsius degree, higher than critical threshold 80",
        "Component": "Temperature",
        "EventTimestamp": "2021-07-09 16:03:36",
        "EventType": "Thermal over threshold",
        "DeviceId": 0,
        "DeviceType": "GPU",
        "UUID": "00000000-0000-0000-0000-020a00008086"
    },
    {
        "EventId": 1,
        "Severity": "Critical",
        "Message": "Off the bus error",
        "Component": "System",
        "EventTimestamp": "2021-07-09 16:03:36",
        "EventType": "Off bus error"
    }
]
```

### Remove device events

Remove device events.

#### Request URL
/rest/v1/events

#### HTTP request method
DELETE

#### Request field description
| Field                    | Type      | R/O      | Description                  |
| ------------------------ | --------- | -------- | ---------------------------- |
| BeginTime                | Timestamp | Optional |                              |
| EndTime                  | Timestamp | Optional |                              |
| Severity                 | String    | Optional | Filter events by severity    |
| Component                | String    | Optional | Filter events by component   |
| EventType                | String    | Optional | Filter events by event type  |
| DeviceType               | String    | Optional | Filter events by device type |
| DeviceId                 | Number    | Optional | Filter events by device id   |
| Pagination parameters... |           |          |                              |
| Sort parameters...       |           |          |                              |

#### Request example
http://localhost:5000/rest/v1/events?DeviceType=GPU&DeviceId=0

#### Response field description
N/A

#### Response example
N/A

## Configuration

### Get device configuration

Get device configuration.

#### Request URL
/rest/v1/devices/{deviceId}/configuration

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/devices/0/configuration

#### Response field description
| Field             | Type   | Description                                                  |
| ----------------- | ------ | ------------------------------------------------------------ |
| DeviceId          | Number | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType        | String | Now is just GPU                                              |
| UUID              | String | A unique device identifier                                   |
| PowerLimit        | Number | In watts(W)                                                  |
| PerformanceFactor | Number | Set GPU performance factor (compute/media)                   |

#### Response example
```json
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "PowerLimit": 300,
    "PerformanceFactor": 1
}
```

### Set device configuration

Set device configuration.

#### Request URL
/rest/v1/devices/{deviceId}/configuration

#### HTTP request method
PUT

#### Request field description
| Field             | Type   | R/O      | Description                                |
| ----------------- | ------ | -------- | ------------------------------------------ |
| PowerLimit        | Number | Optional | In watts(W)                                |
| PerformanceFactor | Number | Optional | Set GPU performance factor (compute/media) |

#### Request example
http://localhost:5000/rest/v1/devices/0/configuration

```json
{
    "PowerLimit":350
}
```

#### Response field description
| Field             | Type   | Description                                                  |
| ----------------- | ------ | ------------------------------------------------------------ |
| DeviceId          | Number | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType        | String | Now is just GPU                                              |
| UUID              | String | A unique device identifier                                   |
| PowerLimit        | Number | In watts(W)                                                  |
| PerformanceFactor | Number | Set GPU performance factor (compute/media)                   |

#### Response example
```json
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "PowerLimit": 350,    
    "PerformanceFactor": 1
}
```

## Configuration by group

### Get group configuration

Get group configuration.

#### Request URL
/rest/v1/groups/{groupId}/configuration

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/groups/0/configuration

#### Response field description
| Field             | Type   | Description                                                  |
| ----------------- | ------ | ------------------------------------------------------------ |
| GroupId           | Number | Group Id                                                     |
| GroupName         | String | Group Name                                                   |
| Configurations    | List   | Configurations for different devices                         |
| DeviceId          | Number | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType        | String | Now is just GPU                                              |
| UUID              | String | A unique device identifier                                   |
| PowerLimit        | Number | In watts(W)                                                  |
| PerformanceFactor | Number | Set GPU performance factor (compute/media)                   |

#### Response example
```json
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Configurations": [
        {
            "DeviceId": 0,
            "Device Type": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "Power Limit": 300,
            "Performance Factor": 1
        }
    ]
}
```

### Set group configuration

Set device configuration.

#### Request URL
/rest/v1/groups/{groupId}/configuration

#### HTTP request method
PUT

#### Request field description
| Field             | Type   | R/O      | Description                                |
| ----------------- | ------ | -------- | ------------------------------------------ |
| PowerLimit        | Number | Optional | In watts(W)                                |
| PerformanceFactor | Number | Optional | Set GPU performance factor (compute/media) |

#### Request example
http://localhost:5000/rest/v1/groups/0/configuration

```json
{
    "PowerLimit":350
}
```

#### Response field description
| Field             | Type   | Description                                                  |
| ----------------- | ------ | ------------------------------------------------------------ |
| GroupId           | Number | Group Id                                                     |
| GroupName         | String | Group Name                                                   |
| Configurations    | List   | Configurations for different devices                         |
| DeviceId          | Number | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType        | String | Now is just GPU                                              |
| UUID              | String | A unique device identifier                                   |
| PowerLimit        | Number | In watts(W)                                                  |
| PerformanceFactor | Number | Set GPU performance factor (compute/media)                   |

#### Response example
```json
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Configurations": [
        {
            "DeviceId": 0,
            "DeviceType": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "PowerLimit": 350,    
            "PerformanceFactor": 1
        }
    ]
}
```


## Firmware update

### Add firmware update task

Update firmware.

#### Request URL
/rest/v1/devices/{deviceId}/firmware

#### HTTP request method
POST

#### Request field description
| Field                     | Type    | R/O      | Description                                                  |
| ------------------------- | ------- | -------- | ------------------------------------------------------------ |
| RestartServerWhenFinished | Boolean | Optional | Whether to restart server when update finish, default false  |
| Tasks                     | Object  | Optional | Firmware update tasks                                        |
| FirmwareName              | String  | Required | The firmware to flash                                        |
| Remote                    | Object  | Optional | The remote firmware file info                                |
| Local                     | Object  | Optional | The local firmware file info                                 |
| FilePath                  | String  | Required | The local firmware file path                                 |
| ImageURI                  | String  | Required | URI referencing a software image to be retrieved             |
| Password                  | String  | Optional | A string representing the password to be used when accessing the URI specified by the Image URI parameter |
| Username                  | String  | Optional | A string representing the username to be used when accessing the URI specified by the Image URI parameter |
| TransferProtocol          | Enum    | Optional | Network protocol used to retrieve the software image located at the Image URI, default HTTP |

#### Request example
http://localhost:5000/rest/v1/devices/0/firmware

```json
{
    "RestartServerWhenFinished": "true",
    "Tasks": [
        {
            "FirmwareName": "GSC",
            "ImageURI": "http://localhost/image/image.0",
            "Username": "username",
            "Password": "password"
        },
        {
            "FirmwareName": "Firmware2",
            "Local": {
                "FilePath": "/tmp/image.2",
            }
        }
    ]
}
```

#### Response field description
| Field                     | Type      | Description                                                  |
| ------------------------- | --------- | ------------------------------------------------------------ |
| DeviceId                  | Number    | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType                | String    | Now is just GPU                                              |
| UUID                      | String    | A unique device identifier                                   |
| Firmwares                 | List      | Firmwares of one device                                      |
| FirmwareName              | String    |                                                              |
| FirmwareVersion           | String    |                                                              |
| UpdateRunning             | Boolean   | Whether there is firmware update task running                |
| Task                      | Object    | Info about firmware update task, present when a task is scheduled |
| Remote                    | Object    | Optional                                                     |
| Local                     | Object    | The local firmware file info                                 |
| FilePath                  | String    | The local firmware file path                                 |
| ImageURI                  | String    | URI referencing a software image to be retrieved             |
| RestartServerWhenFinished | Boolean   | Whether to restart server when update finish, default false  |
| TaskStartTime             | Timestamp | When the update task start                                   |

#### Response example
```json
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "RestartServerWhenFinished": "true",
    "Firmwares": [
        {
            "FirmwareName": "GSC",
            "FirmwareVersion": "ATS1_0.7178",
            "UpdateRunning": "true",
            "Task": {
                "Remote": {
                    "ImageURI": "http://localhost/image/image.0"
                },
                "TaskStartTime": "2021-07-09 16:03:36"
            }
        },
        {
            "FirmwareName": "Firmware1",
            "FirmwareVersion": "1.00",
            "Update Running": "false"
        },
        {
            "FirmwareName": "Firmware2",
            "FirmwareVersion": "2.01",
            "UpdateRunning": "true",
            "Task": {
                "Local": {
                    "ImageURI": "/tmp/image.2",
                },
                "TaskStartTime": "2021-07-09 16:03:36"
            }
        }
    ]
}
```

### Get firmware

Get firmware update task status.

#### Request URL
/rest/v1/devices/{deviceId}/firmware

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/devices/0/firmware


#### Response field description
| Field                     | Type      | Description                                                  |
| ------------------------- | --------- | ------------------------------------------------------------ |
| DeviceId                  | Number    | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType                | String    | Now is just GPU                                              |
| UUID                      | String    | A unique device identifier                                   |
| Firmwares                 | List      | Firmwares of one device                                      |
| FirmwareName              | String    |                                                              |
| FirmwareVersion           | String    |                                                              |
| UpdateRunning             | Boolean   | Whether there is firmware update task running                |
| Task                      | Object    | Info about firmware update task, present when a task is scheduled |
| ImageURI                  | String    | URI referencing a software image to be retrieved             |
| RestartServerWhenFinished | Boolean   | Whether to restart server when update finish, default false  |
| TaskStartTime             | Timestamp | When the update task start                                   |
| Remote                    | Object    | The remote firmware file info                                |
| Local                     | Object    | The local firmware file info                                 |
| FilePath                  | String    | The local firmware file path                                 |

#### Response example
```json
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "Firmwares": [
        {
            "FirmwareName": "GSC",
            "FirmwareVersion": "ATS1_0.7178",
            "UpdateRunning": "true",
            "Task": {
                "Remote": {
                    "ImageURI": "http://localhost/image/image.0",
                },
                "RestartServerWhenFinished": "true",
                "TaskStartTime": "2021-07-09 16:03:36"
            }
        },
        {
            "FirmwareName": "Firmware1",
            "FirmwareVersion": "1.00",
            "UpdateRunning": "false"
        }
    ]
}
```

## Firmware update by group

### Add firmware update task

Update firmware.

#### Request URL
/rest/v1/groups/{groupId}/firmware

#### HTTP request method
POST

#### Request field description
| Field                     | Type    | R/O      | Description                                                  |
| ------------------------- | ------- | -------- | ------------------------------------------------------------ |
| RestartServerWhenFinished | Boolean | Optional | Whether to restart server when update finish, default false  |
| Tasks                     | Object  | Optional | Firmware update tasks                                        |
| FirmwareName              | String  | Required |                                                              |
| Remote                    | Object  | Optional | The remote firmware file info                                |
| Local                     | Object  | Optional | The local firmware file info                                 |
| FilePath                  | String  | Required | The local firmware file path                                 |
| ImageURI                  | String  | Required | URI referencing a software image to be retrieved             |
| Password                  | String  | Optional | A string representing the password to be used when accessing the URI specified by the Image URI parameter |
| Username                  | String  | Optional | A string representing the username to be used when accessing the URI specified by the Image URI parameter |
| TransferProtocol          | Enum    | Optional | Network protocol used to retrieve the software image located at the Image URI, default HTTP |

#### Request example
http://localhost:5000/rest/v1/groups/0/firmware

```json
{
    "RestartServerWhenFinished": "true",
    "Tasks": [
        {
            "FirmwareName": "GSC",
            "ImageURI": "http://localhost/image/image.0",
            "Username": "username",
            "Password": "password"
        },
        {
            "FirmwareName": "Firmware2",
            "Local": {
                "FilePath": "/tmp/image.2",
            }
        }
    ]
}
```

#### Response field description
| Field                     | Type      | Description                                                  |
| ------------------------- | --------- | ------------------------------------------------------------ |
| GroupId                   | Number    | Group Id                                                     |
| GroupName                 | String    | Group Name                                                   |
| DeviceFirmwares           | List      | Firmware for devices                                         |
| DeviceId                  | Number    | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType                | String    | Now is just GPU                                              |
| UUID                      | String    | A unique device identifier                                   |
| Firmwares                 | List      | Firmwares of one device                                      |
| FirmwareName              | String    |                                                              |
| FirmwareVersion           | String    |                                                              |
| UpdateRunning             | Boolean   | Whether there is firmware update task running                |
| Task                      | Object    | Info about firmware update task, present when a task is scheduled |
| Remote                    | Object    | Optional                                                     |
| Local                     | Object    | The local firmware file info                                 |
| FilePath                  | String    | The local firmware file path                                 |
| ImageURI                  | String    | URI referencing a software image to be retrieved             |
| RestartServerWhenFinished | Boolean   | Whether to restart server when update finish, default false  |
| TaskStartTime             | Timestamp | When the update task start                                   |
| ProgressPercentage?       |           |                                                              |

#### Response example
```json
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "DeviceFirmwares": [
        {
            "DeviceId": 0,
            "DeviceType": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "RestartServerWhenFinished": "true",
            "Firmwares": [
                {
                    "FirmwareName": "GSC",
                    "FirmwareVersion": "ATS1_0.7178",
                    "UpdateRunning": "true",
                    "Task": {
                        "Remote": {
                            "ImageURI": "http://localhost/image/image.0",
                        },
                        "TaskStartTime": "2021-07-09 16:03:36"
                    }
                },
                {
                    "FirmwareName": "Firmware1",
                    "FirmwareVersion": "1.00",
                    "Update Running": "false"
                },
                {
                    "FirmwareName": "Firmware2",
                    "FirmwareVersion": "2.01",
                    "UpdateRunning": "true",
                    "Task": {
                        "Local": {
                            "FilePath": "/tmp/image.2"
                        },
                        "TaskStartTime": "2021-07-09 16:03:36"
                    }
                }
            ]
        }
    ]
}
```

### Get firmware by group

Get group firmware update task status.

#### Request URL
/rest/v1/groups/{groupId}/firmware

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/groups/0/firmware


#### Response field description
| Field                     | Type      | Description                                                  |
| ------------------------- | --------- | ------------------------------------------------------------ |
| GroupId                   | Number    | Group Id                                                     |
| GroupName                 | String    | Group Name                                                   |
| DeviceFirmwares           | List      | Firmware for devices                                         |
| DeviceId                  | Number    | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType                | String    | Now is just GPU                                              |
| UUID                      | String    | A unique device identifier                                   |
| Firmwares                 | List      | Firmwares of one device                                      |
| FirmwareName              | String    |                                                              |
| FirmwareVersion           | String    |                                                              |
| UpdateRunning             | Boolean   | Whether there is firmware update task running                |
| Task                      | Object    | Info about firmware update task, present when a task is scheduled |
| Remote                    | Object    | Optional                                                     |
| Local                     | Object    | The local firmware file info                                 |
| FilePath                  | String    | The local firmware file path                                 |
| ImageURI                  | String    | URI referencing a software image to be retrieved             |
| RestartServerWhenFinished | Boolean   | Whether to restart server when update finish, default false  |
| TaskStartTime             | Timestamp | When the update task start                                   |
| Progress Percentage?      |           |                                                              |

#### Response example
```json
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "DeviceFirmwares": [
        {
            "DeviceId": 0,
            "DeviceType": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "Firmwares": [
                {
                    "FirmwareName": "GSC",
                    "FirmwareVersion": "ATS1_0.7178",
                    "UpdateRunning": "true",
                    "Task": {
                        "Remote": {
                            "ImageURI": "http://localhost/image/image.0",
                        },
                        "RestartServerWhenFinished": "true",
                        "TaskStartTime": "2021-07-09 16:03:36"
                    }
                },
                {
                    "FirmwareName": "Firmware1",
                    "FirmwareVersion": "1.00",
                    "UpdateRunning": "false"
                }
            ]
        }
    ]
}
```


## Diagnostics

### Run diagnostics

Run diagnostics.

#### Request URL
/rest/v1/devices/{deviceId}/diagnostics

#### HTTP request method
POST

#### Request field description
| Field            | Type   | R/O      | Description                                                  |
| ---------------- | ------ | -------- | ------------------------------------------------------------ |
| DiagnosticsLevel | Number | Required | Diagnostics Level used to decide what diagnostics is going to run |

#### Request example
http://localhost:5000/rest/v1/devices/0/diagnostics

```json
{
    "DiagnosticsLevel": 1
}
```

#### Response field description
| Field               | Type      | Description                                                  |
| ------------------- | --------- | ------------------------------------------------------------ |
| DeviceId            | Number    | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType          | String    | Now is just GPU                                              |
| UUID                | String    | A unique device identifier                                   |
| DiagnosticsRunning  | Boolean   |                                                              |
| DiagnosticsResult   | Object    |                                                              |
| DiagnosticsLevel    | Number    | Diagnostics Level used to decide what diagnostics is going to run |
| FinishTime          | Timestamp | The time this diagnostics finishes                           |
| BeginTime           | Timestamp | The time when this diagnostics is run                        |
| Status              | Enum      | Diagnostics result, OK\|Warning\|Error                       |
| Description         | String    | Diagnostics result description                               |
| MemoryStatus        | Object    |                                                              |
| PCIeSpeedStatus     | Object    |                                                              |
| Other components... | Object    |                                                              |

#### Response example
```json
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "DiagnosticsRunning": "true",
    "DiagnosticsResult": {
        "DiagnosticsLevel": 1,
        "BeginTime": "2021-07-09 16:03:36",
        "EndTime": null,
        "MemoryStatus": {
            "Status": "OK",
            "Description": "Bandwidth:80GBps"
        }
    }
}
```

### Get diagnostics

Get diagnostics.

#### Request URL
/rest/v1/devices/{deviceId}/diagnostics

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/devices/0/diagnostics

#### Response field description
| Field               | Type      | Description                                                  |
| ------------------- | --------- | ------------------------------------------------------------ |
| DeviceId            | Number    | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType          | String    | Now is just GPU                                              |
| UUID                | String    | A unique device identifier                                   |
| DiagnosticsRunning  | Boolean   |                                                              |
| DiagnosticsResult   | Object    |                                                              |
| DiagnosticsLevel    | Number    | Diagnostics Level used to decide what diagnostics is going to run |
| EndTime             | Timestamp | The time this diagnostics finishes                           |
| BeginTime           | Timestamp | The time when this diagnostics is run                        |
| Status              | Enum      | Diagnostics result, OK\|Warning\|Error                       |
| Description         | String    | Diagnostics result description                               |
| MemoryStatus        | Object    |                                                              |
| PCIeSpeedStatus     | Object    |                                                              |
| Other components... | Object    |                                                              |

#### Response example
```json
// Diagnostics finished
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "DiagnosticsRunning": "false",
    "DiagnosticsResult": {
        "DiagnosticsLevel": 1,
        "BeginTime": "2021-07-09 16:03:36",
        "EndTime": "2021-07-09 16:03:36",
        "MemoryStatus": {
            "Status": "OK",
            "Description": "Bandwidth:80GBps"
        },
        "PCIeSpeedStatus": {
            "Status": "OK",
            "Description": "Speed:1200Mbps"
        }
    }
}

// Diagnostics has never run
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "DiagnosticsRunning": "false",
    "DiagnosticsResult": null
}

// Diagnostics running
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "DiagnosticsRunning": "true",
    "DiagnosticsResult": {
        "DiagnosticsLevel": 1,
        "BeginTime": "2021-07-09 16:03:36",
        "EndTime": null,
        "MemoryStatus": {
            "Status": "OK",
            "Description": "Bandwidth:80GBps"
        }
    }
}
```

### Cancel diagnostics

Get diagnostics.

#### Request URL
/rest/v1/devices/{deviceId}/diagnostics

#### HTTP request method
DELETE

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/devices/0/diagnostics

#### Response field description
| Field               | Type      | Description                                                  |
| ------------------- | --------- | ------------------------------------------------------------ |
| DeviceId            | Number    | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType          | String    | Now is just GPU                                              |
| UUID                | String    | A unique device identifier                                   |
| DiagnosticsRunning  | Boolean   |                                                              |
| DiagnosticsResult   | Object    |                                                              |
| DiagnosticsLevel    | Number    | Diagnostics Level used to decide what diagnostics is going to run |
| EndTime             | Timestamp | The time this diagnostics finishes                           |
| BeginTime           | Timestamp | The time when this diagnostics is run                        |
| Status              | Enum      | Diagnostics result, OK\|Warning\|Error                       |
| Description         | String    | Diagnostics result description                               |
| MemoryStatus        | Object    |                                                              |
| PCIeSpeedStatus     | Object    |                                                              |
| Other components... | Object    |                                                              |

#### Response example
```json
// Diagnostics finished
{
    "DeviceId": 0,
    "DeviceType": "GPU",
    "UUID": "00000000-0000-0000-0000-020a00008086",
    "DiagnosticsRunning": "false",
    "DiagnosticsResult": {
        "DiagnosticsLevel": 1,
        "BeginTime": "2021-07-09 16:03:36",
        "EndTime": "2021-07-09 16:03:36",
        "MemoryStatus": {
            "Status": "OK",
            "Description": "Bandwidth:80GBps"
        },
        "PCIeSpeedStatus": {
            "Status": "OK",
            "Description": "Speed:1200Mbps"
        }
    }
}
```

## Diagnostics by group

### Run diagnostics

Run diagnostics.

#### Request URL
/rest/v1/groups/{groupId}/diagnostics

#### HTTP request method
POST

#### Request field description
| Field            | Type   | R/O      | Description                                                  |
| ---------------- | ------ | -------- | ------------------------------------------------------------ |
| DiagnosticsLevel | Number | Required | Diagnostics Level used to decide what diagnostics is going to run |

#### Request example
http://localhost:5000/rest/v1/groups/0/diagnostics

```json
{
    "DiagnosticsLevel": 1
}
```

#### Response field description
| Field               | Type      | Description                                                  |
| ------------------- | --------- | ------------------------------------------------------------ |
| GroupId             | Number    | Group Id                                                     |
| GroupName           | String    | Group Name                                                   |
| Diagnostics         | List      | Diagnostics status for different devices                     |
| DeviceId            | Number    | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType          | String    | Now is just GPU                                              |
| UUID                | String    | A unique device identifier                                   |
| DiagnosticsRunning  | Boolean   |                                                              |
| DiagnosticsResult   | Object    |                                                              |
| DiagnosticsLevel    | Number    | Diagnostics Level used to decide what diagnostics is going to run |
| FinishTime          | Timestamp | The time this diagnostics finishes                           |
| BeginTime           | Timestamp | The time when this diagnostics is run                        |
| Status              | Enum      | Diagnostics result, OK\|Warning\|Error                       |
| Description         | String    | Diagnostics result description                               |
| MemoryStatus        | Object    |                                                              |
| PCIeSpeedStatus     | Object    |                                                              |
| Other components... | Object    |                                                              |

#### Response example
```json
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Diagnostics": [
        {
            "DeviceId": 0,
            "DeviceType": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "DiagnosticsRunning": "true",
            "DiagnosticsResult": {
                "DiagnosticsLevel": 1,
                "BeginTime": "2021-07-09 16:03:36",
                "EndTime": null,
                "MemoryStatus": {
                    "Status": "OK",
                    "Description": "Bandwidth:80GBps"
                }
            }
        }
    ]
}
```

### Get diagnostics

Get diagnostics.

#### Request URL
/rest/v1/groups/{groupId}/diagnostics

#### HTTP request method
GET

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/groups/0/diagnostics

#### Response field description
| Field               | Type      | Description                                                  |
| ------------------- | --------- | ------------------------------------------------------------ |
| GroupId             | Number    | Group Id                                                     |
| GroupName           | String    | Group Name                                                   |
| Diagnostics         | List      | Diagnostics status for different devices                     |
| DeviceId            | Number    | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType          | String    | Now is just GPU                                              |
| UUID                | String    | A unique device identifier                                   |
| DiagnosticsRunning  | Boolean   |                                                              |
| DiagnosticsResult   | Object    |                                                              |
| DiagnosticsLevel    | Number    | Diagnostics Level used to decide what diagnostics is going to run |
| EndTime             | Timestamp | The time this diagnostics finishes                           |
| BeginTime           | Timestamp | The time when this diagnostics is run                        |
| Status              | Enum      | Diagnostics result, OK\|Warning\|Error                       |
| Description         | String    | Diagnostics result description                               |
| MemoryStatus        | Object    |                                                              |
| PCIeSpeedStatus     | Object    |                                                              |
| Other components... | Object    |                                                              |

#### Response example
```json
// Diagnostics finished
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Diagnostics": [
        {
            "DeviceId": 0,
            "DeviceType": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "DiagnosticsRunning": "false",
            "DiagnosticsResult": {
                "DiagnosticsLevel": 1,
                "BeginTime": "2021-07-09 16:03:36",
                "EndTime": "2021-07-09 16:03:36",
                "MemoryStatus": {
                    "Status": "OK",
                    "Description": "Bandwidth:80GBps",
                },
                "PCIeSpeedStatus": {
                    "Status": "OK",
                    "Description": "Speed:1200Mbps"
                }
            }
        }
    ]
}

// Diagnostics has never run
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Diagnostics": [
        {
            "DeviceId": 0,
            "DeviceType": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "DiagnosticsRunning": "false",
            "DiagnosticsResult": null
        }
    ]
}

// Diagnostics running
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Diagnostics": [
        {
            "DeviceId": 0,
            "DeviceType": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "DiagnosticsRunning": "true",
            "DiagnosticsResult": {
                "DiagnosticsLevel": 1,
                "DiagnosisTime": "2021-07-09 16:03:36",
                "FinishTime": null,
                "MemoryStatus": {
                    "Status": "OK",
                    "Description": "Bandwidth:80GBps"
                }
            }
        }
    ]
}
```

### Cancel diagnostics

Get diagnostics.

#### Request URL
/rest/v1/groups/{groupId}/diagnostics

#### HTTP request method
DELETE

#### Request field description
N/A

#### Request example
http://localhost:5000/rest/v1/groups/0/diagnostics

#### Response field description
| Field               | Type      | Description                                                  |
| ------------------- | --------- | ------------------------------------------------------------ |
| GroupId             | Number    | Group Id                                                     |
| GroupName           | String    | Group Name                                                   |
| Diagnostics         | List      | Diagnostics status for different devices                     |
| DeviceId            | Number    | Device id of this GPU (may change when device un-plug and re-plug) |
| DeviceType          | String    | Now is just GPU                                              |
| UUID                | String    | A unique device identifier                                   |
| DiagnosticsRunning  | Boolean   |                                                              |
| DiagnosticsResult   | Object    |                                                              |
| DiagnosticsLevel    | Number    | Diagnostics Level used to decide what diagnostics is going to run |
| EndTime             | Timestamp | The time this diagnostics finishes                           |
| BeginTime           | Timestamp | The time when this diagnostics is run                        |
| Status              | Enum      | Diagnostics result, OK\|Warning\|Error                       |
| Description         | String    | Diagnostics result description                               |
| MemoryStatus        | Object    |                                                              |
| PCIeSpeedStatus     | Object    |                                                              |
| Other components... | Object    |                                                              |

#### Response example
```json
// Diagnostics finished
{
    "GroupId": 0,
    "GroupName": "All Gpus",
    "Diagnostics": [
        {
            "DeviceId": 0,
            "DeviceType": "GPU",
            "UUID": "00000000-0000-0000-0000-020a00008086",
            "DiagnosticsRunning": "false",
            "DiagnosticsResult": {
                "DiagnosticsLevel": 1,
                "BeginTime": "2021-07-09 16:03:36",
                "EndTime": "2021-07-09 16:03:36",
                "MemoryStatus": {
                    "Status": "OK",
                    "Description": "Bandwidth:80GBps"
                },
                "PCIeSpeedStatus": {
                    "Status": "OK",
                    "Description": "Speed:1200Mbps"
                }
            }
        }
    ]
}
```