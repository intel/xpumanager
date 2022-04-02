#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file prometheus_exporter_types.py
#

from enum import Enum, unique


@unique
class PromMetric(Enum):

    # Engine utilization
    xpum_engine_ratio = ('xpum_engine_ratio', 'GPU active time of the elapsed time (in %), per GPU tile')  # nopep8
    xpum_engine_group_ratio = ('xpum_engine_group_ratio', 'Avg utilization of engine group (in %), per GPU tile', ['type'])  # nopep8

    # Power/Energy/Temperature
    xpum_power_watts = ('xpum_power_watts', 'Avg GPU power (in watts), per GPU and per card')  # nopep8
    xpum_energy_joules = ('xpum_energy_joules', 'Total GPU energy consumption since boot (in Joules), per GPU')  # nopep8
    xpum_temperature_celsius = ('xpum_temperature_celsius', 'Avg GPU temperature (in Celsius degree), per tile', ['location'])  # nopep8
    xpum_max_temperature_celsius = ('xpum_max_temperature_celsius', 'Max GPU temperature (in Celsius degree), per tile', ['location'])  # nopep8

    # Frequency
    xpum_frequency_mhz = ('xpum_frequency_mhz', 'Avg (GPU) frequency (in MHz), per GPU tile', ['location', 'type'])  # nopep8
    # xpum_frequency_throttling_ratio = ('xpum_frequency_throttling_ratio', 'Avg frequency throttle ratio (in %), per GPU tile', ['location'])  # nopep8

    # Memory
    xpum_memory_used_bytes = ('xpum_memory_used_bytes', 'Used GPU memory (in bytes), per GPU tile')  # nopep8
    xpum_memory_ratio = ('xpum_memory_ratio', 'Used GPU memory / Total used GPU memory (in %), per GPU tile')  # nopep8
    xpum_memory_bandwidth_ratio = ('xpum_memory_bandwidth_ratio', 'Avg memory throughput / max memory bandwidth (in %), per GPU tile')  # nopep8
    xpum_memory_read_bytes = ('xpum_memory_read_bytes', 'Total memory read bytes (in bytes), per GPU tile')  # nopep8
    xpum_memory_write_bytes = ('xpum_memory_write_bytes', 'Total memory write bytes (in bytes), per GPU tile')  # nopep8

    # Errors
    xpum_resets = ('xpum_resets', 'Total number of GPU reset since Sysman init, per GPU')  # nopep8
    xpum_programming_errors = ('xpum_programming_errors', 'Total number of GPU programming errors since Sysman init, per GPU')  # nopep8
    xpum_driver_errors = ('xpum_driver_errors', 'Total number of GPU driver errors since Sysman init, per GPU')  # nopep8
    xpum_cache_errors = ('xpum_cache_errors', 'Total number of GPU cache errors since Sysman init, per GPU', ['type'])  # nopep8
    xpum_non_compute_errors = ('xpum_non_compute_errors', 'Total number of GPU non-compute errors since Sysman init, per GPU', ['type'])  # nopep8
    # xpum_display_errors = ('xpum_display_errors', 'Total number of GPU display errors since Sysman init, per GPU', ['type'])  # nopep8

    # Eu Active Stall Idle
    xpum_eu_active_ratio = ('xpum_eu_active_ratio', 'GPU EU Array Active (in %), the normalized sum of all cycles on all EUs that were spent actively executing instructions. Per tile.')  # nopep8
    xpum_eu_stall_ratio = ('xpum_eu_stall_ratio', 'GPU EU Array Stall (in %), the normalized sum of all cycles on all EUs during which the EUs were stalled. Per tile. At least one thread is loaded, but the EU is stalled. Per tile.')  # nopep8
    xpum_eu_idle_ratio = ('xpum_eu_idle_ratio', 'GPU EU Array Idle (in %), the normalized sum of all cycles on all cores when no threads were scheduled on a core. Per tile.')  # nopep8

    # PCIe
    xpum_pcie_read_bytes = ('xpum_pcie_read_bytes', 'Total PCIe read bytes (in bytes), per GPU')  # nopep8
    xpum_pcie_write_bytes = ('xpum_pcie_write_bytes', 'Total PCIe write bytes (in bytes), per GPU')  # nopep8

    # Per Engine Utilization
    xpum_per_engine_ratio = ('xpum_per_engine_ratio', 'Per-engine utilization (in %)', ['type', 'engine_id'])  # nopep8

    # XE Link
    xpum_fabric_tx_bytes = ('xpum_fabric_tx_bytes', 'Data transmitted through fabric link (in bytes)', ['sub_dev', 'dst_dev_file', 'dst_pci_bdf', 'dst_sub_dev'])  # nopep8

    def __new__(cls, name, desc=None, ext_labelnames=[]):
        obj = object.__new__(cls)
        obj._value_ = name
        obj.desc = f'{name}_desc' if desc is None else desc
        obj.ext_labelnames = ext_labelnames
        return obj


class Metric:
    def __init__(self, prom_metric: PromMetric, is_counter: bool = False, process_counter_wrap: bool = True, xpum_field=None, scale: float = 1, ext_labels: dict = {}) -> None:
        self.prom_metric = prom_metric
        self.scale = scale
        self.ext_labels = ext_labels
        self.is_counter = is_counter
        self.process_counter_wrap = process_counter_wrap
        self.xpum_field = xpum_field if xpum_field is not None else (
            'acc' if is_counter else 'avg')


metrics_map = {
    # Engine utilization
    'XPUM_STATS_GPU_UTILIZATION': Metric(PromMetric.xpum_engine_ratio, scale=0.01),
    'XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION': Metric(PromMetric.xpum_engine_group_ratio, scale=0.01, ext_labels={'type': 'compute'}),
    'XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION': Metric(PromMetric.xpum_engine_group_ratio, scale=0.01, ext_labels={'type': 'media'}),
    'XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION': Metric(PromMetric.xpum_engine_group_ratio, scale=0.01, ext_labels={'type': 'copy'}),
    'XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION': Metric(PromMetric.xpum_engine_group_ratio, scale=0.01, ext_labels={'type': 'render'}),
    'XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION': Metric(PromMetric.xpum_engine_group_ratio, scale=0.01, ext_labels={'type': '3d'}),

    # EuActive/EuStall/EuIdle
    'XPUM_STATS_EU_ACTIVE': Metric(PromMetric.xpum_eu_active_ratio, scale=0.01),
    'XPUM_STATS_EU_STALL': Metric(PromMetric.xpum_eu_stall_ratio, scale=0.01),
    'XPUM_STATS_EU_IDLE': Metric(PromMetric.xpum_eu_idle_ratio, scale=0.01),

    # Power/Energy/Temperature
    'XPUM_STATS_POWER': Metric(PromMetric.xpum_power_watts),
    'XPUM_STATS_ENERGY': Metric(PromMetric.xpum_energy_joules, is_counter=True, scale=0.001),
    'XPUM_STATS_GPU_CORE_TEMPERATURE': [
        Metric(PromMetric.xpum_temperature_celsius, ext_labels={'location': 'gpu'}),  # nopep8
        Metric(PromMetric.xpum_max_temperature_celsius, xpum_field='max', ext_labels={'location': 'gpu'})],  # nopep8
    'XPUM_STATS_MEMORY_TEMPERATURE': [
        Metric(PromMetric.xpum_temperature_celsius, ext_labels={'location': 'mem'}),  # nopep8
        Metric(PromMetric.xpum_max_temperature_celsius, xpum_field='max', ext_labels={'location': 'mem'})],  # nopep8

    # Frequency
    'XPUM_STATS_GPU_FREQUENCY': Metric(PromMetric.xpum_frequency_mhz, ext_labels={'location': 'gpu', 'type': 'actual'}),
    'XPUM_STATS_GPU_REQUEST_FREQUENCY': Metric(PromMetric.xpum_frequency_mhz, ext_labels={'location': 'gpu', 'type': 'request'}),
    # 'XPUM_STATS_GPU_FREQUENCY_THROTTLE_RATIO': Metric(PromMetric.xpum_frequency_throttling_ratio, scale=0.01, ext_labels={'location': 'gpu'}),

    # Memory
    'XPUM_STATS_MEMORY_USED': Metric(PromMetric.xpum_memory_used_bytes),
    'XPUM_STATS_MEMORY_UTILIZATION': Metric(PromMetric.xpum_memory_ratio, scale=0.01),
    'XPUM_STATS_MEMORY_BANDWIDTH': Metric(PromMetric.xpum_memory_bandwidth_ratio, scale=0.01),
    'XPUM_STATS_MEMORY_READ': Metric(PromMetric.xpum_memory_read_bytes, is_counter=True),  # nopep8
    'XPUM_STATS_MEMORY_WRITE': Metric(PromMetric.xpum_memory_write_bytes, is_counter=True),  # nopep8

    # Errors
    'XPUM_STATS_RAS_ERROR_CAT_RESET': Metric(PromMetric.xpum_resets, is_counter=True),
    'XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS': Metric(PromMetric.xpum_programming_errors, is_counter=True),
    'XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS': Metric(PromMetric.xpum_driver_errors, is_counter=True),
    'XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE': Metric(PromMetric.xpum_cache_errors, is_counter=True, ext_labels={'type': 'correctable'}),
    'XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE': Metric(PromMetric.xpum_cache_errors, is_counter=True, ext_labels={'type': 'uncorrectable'}),
    'XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE': Metric(PromMetric.xpum_non_compute_errors, is_counter=True, ext_labels={'type': 'correctable'}),
    'XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE': Metric(PromMetric.xpum_non_compute_errors, is_counter=True, ext_labels={'type': 'uncorrectable'}),

    # PCIe
    'XPUM_STATS_PCIE_READ': Metric(PromMetric.xpum_pcie_read_bytes, is_counter=True),  # nopep8
    'XPUM_STATS_PCIE_WRITE': Metric(PromMetric.xpum_pcie_write_bytes, is_counter=True),  # nopep8

    # Per Engine Utilization
    'XPUM_STATS_ENGINE_UTILIZATION': Metric(PromMetric.xpum_per_engine_ratio, scale=0.01, ext_labels={'type': '$engine_type', 'engine_id': '$engine_id'}),  # nopep8

    # XE Link
    'XPUM_STATS_FABRIC_THROUGHPUT': Metric(PromMetric.xpum_fabric_tx_bytes, is_counter=True, ext_labels={'sub_dev': '$src_tile_id', 'dst_dev_file': '$dst_dev_file', 'dst_pci_bdf': '$dst_pci_bdf', 'dst_sub_dev': '$dst_tile_id'})  # nopep8
}


def avg(x):
    return sum(x)/len(x) if len(x) > 0 else 0


tile_to_device_aggregators = {
    'XPUM_STATS_POWER': {'avg': sum},
    'XPUM_STATS_RAS_ERROR_CAT_RESET': {'acc': sum},
    'XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS': {'acc': sum},
    'XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS': {'acc': sum},
    'XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE': {'acc': sum},
    'XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE': {'acc': sum},
    'XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE': {'acc': sum},
    'XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE': {'acc': sum},
    'XPUM_STATS_MEMORY_UTILIZATION': {'avg': avg},
    'XPUM_STATS_MEMORY_BANDWIDTH': {'avg': avg},
    'XPUM_STATS_GPU_UTILIZATION': {'avg': avg}
}

device_to_card_aggregators = {
    'XPUM_STATS_POWER': {'avg': sum},
}
