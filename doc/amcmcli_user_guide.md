# Intel(R) XPU Manager - AMC Manager Command Line Interface User Guide
This guide describes how to use amcmcli to manage Intel data center GPU AMC firmware. amcmcli is a portable CLI tool to manage Intel data center GPU AMC (Add-in-card management controller) firmware in Linux OS. It is independent of the GPU driver and need not installed. This tool can work on Intel M50CYP server (BMC firmware version is 2.82 or newer), H3C R5300 G5, Inspur NF5280M6 and other servers which enable AMC through IPMI protocol. 

## Main features
* Show all AMC firmware versions
* Update all AMC firmware to the specified version (the installed GPUs should be the same model)

## Help info
Show the tool help info
```
amcmcli
Usage: ./amcmcli [OPTIONS] [SUBCOMMAND]

Options:
  -h,--help                   Print this help message and exit
  -v,--version                List version info

Subcommands:
  fwversion                   List all AMC firmware versions
  updatefw                    Update all AMC firmware to the specified version
```

Show the tool version
```
amcmcli -v
Version: 1.2.1.20230112
Build ID: 9cf513d1
```

## Show all AMC firmware versions
```
amcmcli fwversion
3 AMC are found
AMC 0 firmware version: 6.1.0.0
AMC 1 firmware version: 6.1.0.0
AMC 2 firmware version: 6.1.0.0
```

## Update all AMC firmware to the specified version
Help info the this feature.
```
amcmcli updatefw -h
Update all AMC firmware to the specified version
Usage: ./amcmcli updatefw [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -y,--assumeyes              Assume that the answer to any question which would be asked is yes
  -f                          AMC firmware filename
```

Update all AMC firmware to the specified version
```
amcmcli updatefw -f ats_m_amc_v_6_1_0_0.bin
CAUTION: it will update the AMC firmware of all cards and please make sure that you install the GPUs of the same model.
Please confirm to proceed (y/n) y
Start to update firmware
Firmware Name: AMC
Image path: /home/gta/ats_m_amc_v_6_1_0_0.bin
[============================================================] 100 %
Update firmware successfully.
```