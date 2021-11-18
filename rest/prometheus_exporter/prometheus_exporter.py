from prometheus_client import CollectorRegistry, Gauge, Counter, generate_latest

import os
import traceback
import sys

from enum import Enum, unique

registries = {}
counter_values = {}


@unique
class PromMetric(Enum):

    # Engine utilization
    xpum_engine_ratio = ('xpum_engine_ratio', 'Max utilization among all engine groups (in %), per GPU tile')  # nopep8
    xpum_engine_group_ratio = ('xpum_engine_group_ratio', 'Avg utilization of engine group (in %), per GPU tile', ['type'])  # nopep8

    # Power/Energy/Temperature
    xpum_power_watts = ('xpum_power_watts', 'Avg GPU power (in watts), per GPU and per card')  # nopep8
    xpum_energy_joules = ('xpum_energy_joules', 'Total GPU energy consumption since boot (in Joules), per GPU')  # nopep8
    xpum_temperature_celsius = ('xpum_temperature_celsius', 'Avg GPU temperature (in Celsius degree), per tile', ['location'])  # nopep8

    # Frequency
    xpum_frequency_mhz = ('xpum_frequency_mhz', 'Avg (GPU) frequency (in MHz), per GPU tile', ['location', 'type'])  # nopep8
    xpum_frequency_throttling_ratio = ('xpum_frequency_throttling_ratio', 'Avg frequency throttle ratio (in %), per GPU tile', ['location'])  # nopep8

    # Memory
    xpum_memory_used_bytes = ('xpum_memory_used_bytes', 'Used GPU memory (in bytes), per GPU tile')  # nopep8
    xpum_memory_ratio = ('xpum_memory_ratio', 'Used GPU memory / Total used GPU memory (in %), per GPU tile')  # nopep8
    xpum_memory_bandwidth_ratio = ('xpum_memory_bandwidth_ratio', 'Avg memory throughput / max memory bandwidth (in %), per GPU tile')  # nopep8
    xpum_memory_read_bytes = ('xpum_memory_read_bytes', 'Total memory read bytes (in bytes), per GPU tile')  # nopep8
    xpum_memory_write_bytes = ('xpum_memory_write_bytes', 'Total memory write bytes (in bytes), per GPU tile')  # nopep8

    # Errors
    xpum_resets = ('xpum_resets', 'Total number of GPU reset since boot, per GPU')  # nopep8
    xpum_programming_errors = ('xpum_programming_errors', 'Total number of GPU programming errors since boot, per GPU')  # nopep8
    xpum_driver_errors = ('xpum_driver_errors', 'Total number of GPU driver errors since boot, per GPU')  # nopep8
    xpum_cache_errors = ('xpum_cache_errors', 'Total number of GPU cache errors since boot, per GPU', ['type'])  # nopep8
    xpum_display_errors = ('xpum_display_errors', 'Total number of GPU display errors since boot, per GPU', ['type'])  # nopep8

    # Eu Active Stall Idle
    xpum_eu_active_ratio = ('xpum_eu_active_ratio')  # nopep8
    xpum_eu_stall_ratio = ('xpum_eu_stall_ratio')  # nopep8
    xpum_eu_idle_ratio = ('xpum_eu_idle_ratio')  # nopep8

    def __new__(cls, name, desc=None, ext_labelnames=[]):
        obj = object.__new__(cls)
        obj._value_ = name
        obj.desc = f'{name}_desc' if desc is None else desc
        obj.ext_labelnames = ext_labelnames
        return obj


class Metric:
    def __init__(self, prom_metric: PromMetric, scale: float = 1, ext_labels: dict = {}) -> None:
        self.prom_metric = prom_metric
        self.scale = scale
        self.ext_labels = ext_labels


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
    'XPUM_STATS_ENERGY': Metric(PromMetric.xpum_energy_joules, scale=0.001),
    'XPUM_STATS_GPU_CORE_TEMPERATURE': Metric(PromMetric.xpum_temperature_celsius, ext_labels={'location': 'gpu'}),

    # Frequency
    'XPUM_STATS_GPU_FREQUENCY': Metric(PromMetric.xpum_frequency_mhz, ext_labels={'location': 'gpu', 'type': 'actual'}),
    'XPUM_STATS_GPU_REQUEST_FREQUENCY': Metric(PromMetric.xpum_frequency_mhz, ext_labels={'location': 'gpu', 'type': 'request'}),
    'XPUM_STATS_GPU_FREQUENCY_THROTTLE_RATIO': Metric(PromMetric.xpum_frequency_throttling_ratio, scale=0.01, ext_labels={'location': 'gpu'}),

    # Memory
    'XPUM_STATS_MEMORY_USED': Metric(PromMetric.xpum_memory_used_bytes),
    'XPUM_STATS_MEMORY_UTILIZATION': Metric(PromMetric.xpum_memory_ratio, scale=0.01),
    'XPUM_STATS_MEMORY_BANDWIDTH': Metric(PromMetric.xpum_memory_bandwidth_ratio, scale=0.01),
    'XPUM_STATS_MEMORY_READ': Metric(PromMetric.xpum_memory_read_bytes),
    'XPUM_STATS_MEMORY_WRITE': Metric(PromMetric.xpum_memory_write_bytes),

    # Errors
    'XPUM_STATS_RAS_ERROR_CAT_RESET': Metric(PromMetric.xpum_resets),
    'XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS': Metric(PromMetric.xpum_programming_errors),
    'XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS': Metric(PromMetric.xpum_driver_errors),
    'XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE': Metric(PromMetric.xpum_cache_errors, ext_labels={'type': 'correctable'}),
    'XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE': Metric(PromMetric.xpum_cache_errors, ext_labels={'type': 'uncorrectable'}),
    'XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE': Metric(PromMetric.xpum_display_errors, ext_labels={'type': 'correctable'}),
    'XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE': Metric(PromMetric.xpum_display_errors, ext_labels={'type': 'uncorrectable'}),
}


def get_metrics(core, pod_resources):

    try:
        code, _, data = core.getDeviceList()
        if code != 0:
            return '#NODATA', 500

        resp = b''

        for dev in data:

            device_id = dev.get('device_id')
            stat_code, _, stat_data = core.getStatistics(
                device_id, session_id=1, get_accumulated=True)

            if stat_code != 0:
                continue

            if 'device_level' in stat_data:
                r = convert_to_prometheus_metrics(
                    pod_resources, dev, stat_data['device_level'], device_id)
                resp = resp + r

            for tile_data in stat_data.get('tile_level', []):
                r = convert_to_prometheus_metrics(
                    pod_resources, dev, tile_data['data_list'], device_id, tile_data['tile_id'])
                resp = resp + r

        code, _, data = core.getAllGroups()
        if code != 0:
            return '#NODATA', 500

        for group in data:
            group_id = group.get('group_id')
            # only process built-in groups (i.e., cards) whose group id has 1 as the highest bit
            if group_id is None or group_id & 0x80000000 != 0x80000000:
                continue

            stat_code, _, stat_data = core.getStatisticsByGroup(
                group_id, session_id=1, get_accumulated=True)
            if stat_code != 0:
                continue
            card_power = 0
            for datas in stat_data.get('datas', []):
                for metric in datas.get('device_level', {}):
                    if metric['metrics_type'] == 'XPUM_STATS_POWER':
                        card_power = card_power + metric.get('value')
                        break

            card_data = [{'metrics_type': 'XPUM_STATS_POWER', 'avg': card_power} ]
            r = convert_to_prometheus_metrics(
                pod_resources, dev=None, datalist=card_data, card_id=group_id)
            resp = resp + r

        return tidy_response(resp)
    except Exception as e:
        traceback.print_exc()
        return "#NODATA due to unexpected failure", 500


def tidy_response(resp):
    resp_str = resp.decode('UTF-8')
    comments = []
    metrics = []
    for line in resp_str.splitlines():
        if not line.startswith('#'):
            metrics.append(line)
        elif line not in comments:
            comments.append(line)
    return '\n'.join(comments + metrics)


def convert_to_prometheus_metrics(pod_resources, dev, datalist, device_id=None, tile_id=None, card_id=None):
    labels, label_values = build_dev_labels(dev)
    attach_kube_labels(dev, labels, label_values, pod_resources)
    attach_tile_labels(labels, label_values, tile_id)
    attach_card_labels(labels, label_values, card_id)

    metrics_owner = f'card:{card_id}_gpu:{device_id}_tile:{tile_id}'
    registry, metrics = registries.setdefault(
        metrics_owner, (CollectorRegistry(), {}))

    for stat in datalist:
        metric = metrics_map.get(stat.get('metrics_type'))
        if metric is None:
            continue
        metric_name = metric.prom_metric.name
        scale = metric.scale
        acc = stat.get('acc')
        avg = stat.get('avg')

        all_labelnames, all_labelvalues = attach_ext_labels(
            labels, label_values, metric.prom_metric.ext_labelnames, metric.ext_labels)

        if metric_name not in metrics:
            if acc is not None:
                # counter value
                metrics[metric_name] = Counter(
                    metric_name, metric.prom_metric.desc, labelnames=all_labelnames, registry=registry)
                mm = metrics[metric_name].labels(*all_labelvalues)
                new_value = acc * scale
                counter_values[mm] = new_value
                mm.inc(new_value)
            else:
                # aggregated value
                metrics[metric_name] = Gauge(
                    metric_name, metric.prom_metric.desc, labelnames=all_labelnames, registry=registry)
                metrics[metric_name].labels(*all_labelvalues).set(avg * scale)
        else:
            if acc is not None:
                # counter value
                mm = metrics[metric_name].labels(*all_labelvalues)
                cur_value = counter_values.setdefault(mm, 0)
                new_value = acc * scale
                if new_value >= cur_value:
                    counter_values[mm] = new_value
                    mm.inc(new_value-cur_value)
                else:
                    print(f'counter value decreased, {metric_name}: pre={cur_value}, cur={new_value}')
            else:
                # aggregated value
                metrics[metric_name].labels(*all_labelvalues).set(avg * scale)
    return generate_latest(registry)


def build_dev_labels(dev):
    if dev is None:
        return [], []

    labels = ['uuid', 'dev_name', 'pci_dev', 'vendor', 'pci_bdf']
    label_values = [dev.get(key, '') for key in [
        'uuid', 'device_name', 'pci_device_id', 'vendor_name', 'pci_bdf_address']]

    return labels, label_values


def attach_kube_labels(dev, labels, label_values, pod_resources):
    nodename = os.getenv('NODE_NAME')
    if nodename is not None:
        labels.append('node')
        label_values.append(nodename)

    if dev is None or pod_resources is None:
        return
    bdf = dev.get('pci_bdf_address', '')
    pod_resource = pod_resources.get(bdf, {})
    if 'pod' in pod_resource:
        labels.append('kube_pod')
        label_values.append(pod_resource['pod'])
    if 'namespace' in pod_resource:
        labels.append('kube_namespace')
        label_values.append(pod_resource['namespace'])
    if 'container' in pod_resource:
        labels.append('kube_container')
        label_values.append(pod_resource['container'])


def attach_tile_labels(labels, label_values, tile_id):
    if tile_id is not None:
        labels.append('sub_dev')
        label_values.append(tile_id)


def attach_card_labels(labels, label_values, card_id):
    if card_id is not None:
        labels.append('card')
        label_values.append(card_id)


def attach_ext_labels(labels, label_values, ext_labelnames, ext_labels):
    if ext_labelnames is not None and len(ext_labelnames) > 0:
        all_labelnames = labels + ext_labelnames
        all_labelvalues = label_values + \
            [ext_labels.get(key, 'n/a') for key in ext_labelnames]
        return all_labelnames, all_labelvalues
    return labels, label_values


if __name__ == '__main__':
    rest_folder = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    sys.path.insert(1, rest_folder)
    import stub as core
    from kube_pod_resource import get_pod_resources
    pod_resources = get_pod_resources()
    metrics = get_metrics(core, pod_resources)
    print(metrics)
