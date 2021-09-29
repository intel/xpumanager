from prometheus_client import CollectorRegistry, Gauge, Counter, generate_latest

import os
import traceback

metrics_map = {
    'XPUM_STATS_GPU_UTILIZATION': 'xpum_gpu_ratio',
    'XPUM_STATS_OCCUPATION': 'xpum_occupation',
    'XPUM_STATS_ISSUE_EFFICIENCY': 'xpum_issue_efficiency_ratio',
    'XPUM_STATS_EXECUTION_EFFICIENCY': 'xpum_execution_efficiency_ratio',
    'XPUM_STATS_NON_OCCUPATION': 'xpum_non_occupation_ratio',
    'XPUM_STATS_POWER': 'xpum_power_watts',
    'XPUM_STATS_ENERGY': 'xpum_energy_joules',
    'XPUM_STATS_GPU_FREQUENCY': 'xpum_gpu_frequency_mhz',
    'XPUM_STATS_GPU_TEMEPERATURE': 'xpum_gpu_temeperature_celsius',
    'XPUM_STATS_MEMORY_USED': 'xpum_memory_used_bytes',
    'XPUM_STATS_MEMORY_UTILIZATION': 'xpum_memory_ratio',
    'XPUM_STATS_MEMORY_BANDWIDTH': 'xpum_memory_bandwidth_bytes',
    'XPUM_STATS_MEMORY_READ': 'xpum_memory_read_bytes',
    'XPUM_STATS_MEMORY_WRITE': 'xpum_memory_write_bytes',
    'XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION': 'xpum_engine_group_compute_all_ratio',
    'XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION': 'xpum_engine_group_media_all_ratio',
    'XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION': 'xpum_engine_group_copy_all_ratio',
    'XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION': 'xpum_engine_group_render_all_ratio',
    'XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION': 'xpum_engine_group_3d_all_ratio',
    'XPUM_STATS_RAS_ERROR_CAT_RESET': 'xpum_resets',
    'XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS': 'xpum_programming_errors',
    'XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS': 'xpum_driver_errors',
    'XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE': 'xpum_cache_errors_correctable',
    'XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE': 'xpum_cache_errors_uncorrectable',
    'XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE': 'xpum_display_errors_correctable',
    'XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE': 'xpum_display_errors_uncorrectable'
}


def get_metrics(core, pod_resources):

    try:
        code, _, data = core.getDeviceList()
        if code != 0:
            return '#NODATA'

        resp = b''

        for dev in data:
            stat_code, _, stat_data = core.getStatistics(dev.get('DeviceId'))

            if stat_code != 0:
                continue

            if 'DeviceLevel' in stat_data:
                r = convert_to_prometheus_metrics(
                    pod_resources, dev, stat_data['DeviceLevel'])
                resp = resp + r

            for tile_data in stat_data.get('TileLevel', []):
                r = convert_to_prometheus_metrics(
                    pod_resources, dev, tile_data['dataList'], tile_data['tileId'])
                resp = resp + r

        return tidy_response(resp)
    except Exception as e:
        print(e)
        traceback.print_exc()


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


def convert_to_prometheus_metrics(pod_resources, dev, datalist, tile_id=None):

    labels, label_values = build_basic_labels(dev)
    attach_kube_labels(dev, labels, label_values, pod_resources)
    attach_tile_labels(labels, label_values, tile_id)

    registry = CollectorRegistry()
    metrics = {}

    for stat in datalist:
        metrics_type = metrics_map.get(stat.get('metricsType'))
        if metrics_type is None:
            continue
        value = stat.get('value')
        avg = stat.get('avg')
        if metrics_type not in metrics:
            if avg is not None:
                metrics[metrics_type] = Gauge(
                    metrics_type, f'{metrics_type}_DESCRIPTION', labelnames=labels, registry=registry)
                metrics[metrics_type].labels(*label_values).set(avg)
            else:
                metrics[metrics_type] = Counter(
                    metrics_type, f'{metrics_type}_DESCRIPTION', labelnames=labels, registry=registry)
                metrics[metrics_type].labels(*label_values).inc(value)

    return generate_latest(registry)


def build_basic_labels(dev):
    labels = ['uuid', 'dev_name', 'pci_dev', 'vendor', 'pci_bdf']
    label_values = [dev.get(key, '') for key in [
        'UUID', 'DeviceName', 'PCIDeviceId', 'VendorName', 'PCIBDFAddress']]

    return labels, label_values


def attach_kube_labels(dev, labels, label_values, pod_resources):
    bdf = dev.get('PCIBDFAddress', '')
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

    nodename = os.getenv('NODE_NAME')
    if nodename is not None:
        labels.append('node')
        label_values.append(nodename)


def attach_tile_labels(labels, label_values, tile_id):
    if tile_id is not None:
        labels.append('tile')
        label_values.append(tile_id)


if __name__ == '__main__':
    from DGMCore import DGMCore
    from kube_pod_resource import get_pod_resources
    core = DGMCore()
    pod_resources = get_pod_resources()
    metrics = get_metrics(core, pod_resources)
    print(metrics)
