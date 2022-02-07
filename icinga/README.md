# XPUM Icinga Integration

Assume you have icinga environment setup, just copy `check_intel_gpu.py` into icinga `PluginDir` (`PluginDir` usually can be found in icinga configuration file `constants.conf`). And change `check_intel_gpu.py` file permission to 755.

The gpu.conf is an example of icinga configuration, you can write your own, remember replace placeholders quoted by `$`, like `$HOST_ADDRESS$`.

## check_intel_gpu.py plugin help

```
usage: check_intel_gpu.py [-h] [-V] -H HOST -p PORT -U USERNAME -P PASSWORD -d DEVICEID [--disableTLS] -T {telemetry,health} [--gpu_utilization_warning GPU_UTILIZATION_WARNING]
                          [--gpu_utilization_critical GPU_UTILIZATION_CRITICAL] [--gpu_power_warning GPU_POWER_WARNING] [--gpu_power_critical GPU_POWER_CRITICAL]
                          [--gpu_core_temperature_warning GPU_CORE_TEMPERATURE_WARNING] [--gpu_core_temperature_critical GPU_CORE_TEMPERATURE_CRITICAL]
                          [--gpu_memory_temperature_warning GPU_MEMORY_TEMPERATURE_WARNING] [--gpu_memory_temperature_critical GPU_MEMORY_TEMPERATURE_CRITICAL]
                          [--gpu_energy_consumed_warning GPU_ENERGY_CONSUMED_WARNING] [--gpu_energy_consumed_critical GPU_ENERGY_CONSUMED_CRITICAL]
                          [--driver_errors_warning DRIVER_ERRORS_WARNING] [--driver_errors_critical DRIVER_ERRORS_CRITICAL]
                          [--cache_errors_correctable_warning CACHE_ERRORS_CORRECTABLE_WARNING] [--cache_errors_correctable_critical CACHE_ERRORS_CORRECTABLE_CRITICAL]
                          [--cache_errors_uncorrectable_warning CACHE_ERRORS_UNCORRECTABLE_WARNING] [--cache_errors_uncorrectable_critical CACHE_ERRORS_UNCORRECTABLE_CRITICAL]
                          [--gpu_memory_used_warning GPU_MEMORY_USED_WARNING] [--gpu_memory_used_critical GPU_MEMORY_USED_CRITICAL]

optional arguments:
  -h, --help            show this help message and exit
  -V, --version         show program's version number and exit
  -H HOST, --host HOST  The ip address or hostname of the host that runs XPU Manager Restful API service
  -p PORT, --port PORT  The port of XPU Manager Restful API service
  -U USERNAME, --Username USERNAME
                        The username of XPU Manager Restful API service
  -P PASSWORD, --Password PASSWORD
                        The password
  -d DEVICEID, --deviceId DEVICEID
                        The xpum device id of the gpu
  --disableTLS          Use http instead of https
  -T {telemetry,health}, --Type {telemetry,health}
                        The gpu info type to check
  --gpu_utilization_warning GPU_UTILIZATION_WARNING
                        The warning threshold of GPU Utilization
  --gpu_utilization_critical GPU_UTILIZATION_CRITICAL
                        The critical threshold of GPU Utilization
  --gpu_power_warning GPU_POWER_WARNING
                        The warning threshold of GPU Power
  --gpu_power_critical GPU_POWER_CRITICAL
                        The critical threshold of GPU Power
  --gpu_core_temperature_warning GPU_CORE_TEMPERATURE_WARNING
                        The warning threshold of GPU Core Temperature
  --gpu_core_temperature_critical GPU_CORE_TEMPERATURE_CRITICAL
                        The critical threshold of GPU Core Temperature
  --gpu_memory_temperature_warning GPU_MEMORY_TEMPERATURE_WARNING
                        The warning threshold of GPU Memory Temperature
  --gpu_memory_temperature_critical GPU_MEMORY_TEMPERATURE_CRITICAL
                        The critical threshold of GPU Memory Temperature
  --gpu_energy_consumed_warning GPU_ENERGY_CONSUMED_WARNING
                        The warning threshold of GPU Energy Consumed
  --gpu_energy_consumed_critical GPU_ENERGY_CONSUMED_CRITICAL
                        The critical threshold of GPU Energy Consumed
  --driver_errors_warning DRIVER_ERRORS_WARNING
                        The warning threshold of Driver Errors
  --driver_errors_critical DRIVER_ERRORS_CRITICAL
                        The critical threshold of Driver Errors
  --cache_errors_correctable_warning CACHE_ERRORS_CORRECTABLE_WARNING
                        The warning threshold of Cache Errors Correctable
  --cache_errors_correctable_critical CACHE_ERRORS_CORRECTABLE_CRITICAL
                        The critical threshold of Cache Errors Correctable
  --cache_errors_uncorrectable_warning CACHE_ERRORS_UNCORRECTABLE_WARNING
                        The warning threshold of Cache Errors Uncorrectable
  --cache_errors_uncorrectable_critical CACHE_ERRORS_UNCORRECTABLE_CRITICAL
                        The critical threshold of Cache Errors Uncorrectable
  --gpu_memory_used_warning GPU_MEMORY_USED_WARNING
                        The warning threshold of GPU Memory Used
  --gpu_memory_used_critical GPU_MEMORY_USED_CRITICAL
                        The critical threshold of GPU Memory Used
```