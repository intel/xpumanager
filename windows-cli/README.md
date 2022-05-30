# Intel XPU Manager CLI for Windows
Intel XPU Manager CLI for Windows is daemon-less and has limited features.
It only supports discovery, config, stats and dump now.
Please refer to [XPU Manager CLI User Guide](../doc/CLI_user_guide.md) for how to use the command line tool. 

## Supported Devices
* Intel Data Center GPU

## Supported OS
* Windows Server 2022

## How to build
Visual Studio 16 2019 (Recommended)
  
## How to run
Make sure that level zero has been installed on Windows and then open the Command Prompt as administrator to run xpumcli. 


Note: The binary does not require any special installation. After downloading, please check its signature and checksum. Because its execution need the administrator privilege, it must be kept in a directory which non-admin user have no write permission. We recommend to put it under C:\Windows.