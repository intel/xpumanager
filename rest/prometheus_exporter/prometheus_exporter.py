from prometheus_client import CollectorRegistry, Gauge, Counter, generate_latest

import os
import traceback

metrics_map = {
    'XPUM_STATS_GPU_UTILIZATION': {'name': 'xpum_gpu_ratio', 'scale': 1},
    'XPUM_STATS_OCCUPATION': {'name': 'xpum_occupation_ratio', 'scale': 1},
    'XPUM_STATS_ISSUE_EFFICIENCY': {'name': 'xpum_issue_efficiency_ratio', 'scale': 1},
    'XPUM_STATS_EXECUTION_EFFICIENCY': {'name': 'xpum_execution_efficiency_ratio', 'scale': 1},
    'XPUM_STATS_NON_OCCUPATION': {'name': 'xpum_non_occupation_ratio', 'scale': 1},
    'XPUM_STATS_POWER': {'name': 'xpum_power_watts', 'scale': 1},
    'XPUM_STATS_ENERGY': {'name': 'xpum_energy_joules', 'scale': 0.001},
    'XPUM_STATS_GPU_FREQUENCY': {'name': 'xpum_gpu_frequency_mhz', 'scale': 1},
    'XPUM_STATS_GPU_TEMEPERATURE': {'name': 'xpum_gpu_temeperature_celsius', 'scale': 1},
    'XPUM_STATS_MEMORY_USED': {'name': 'xpum_memory_used_bytes', 'scale': 1},
    'XPUM_STATS_MEMORY_UTILIZATION': {'name': 'xpum_memory_ratio', 'scale': 1},
    'XPUM_STATS_MEMORY_BANDWIDTH': {'name': 'xpum_memory_bandwidth_ratio', 'scale': 1},
    'XPUM_STATS_MEMORY_READ': {'name': 'xpum_memory_read_bytes', 'scale': 1},
    'XPUM_STATS_MEMORY_WRITE': {'name': 'xpum_memory_write_bytes', 'scale': 1},
    'XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION': {'name': 'xpum_engine_group_compute_all_ratio', 'scale': 1},
    'XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION': {'name': 'xpum_engine_group_media_all_ratio', 'scale': 1},
    'XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION': {'name': 'xpum_engine_group_copy_all_ratio', 'scale': 1},
    'XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION': {'name': 'xpum_engine_group_render_all_ratio', 'scale': 1},
    'XPUM_STATS_ENGINE_GROUP_3D_ALL_UTILIZATION': {'name': 'xpum_engine_group_3d_all_ratio', 'scale': 1},
    'XPUM_STATS_RAS_ERROR_CAT_RESET': {'name': 'xpum_resets', 'scale': 1},
    'XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS': {'name': 'xpum_programming_errors', 'scale': 1},
    'XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS': {'name': 'xpum_driver_errors', 'scale': 1},
    'XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE': {'name': 'xpum_cache_errors_correctable', 'scale': 1},
    'XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE': {'name': 'xpum_cache_errors_uncorrectable', 'scale': 1},
    'XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_CORRECTABLE': {'name': 'xpum_display_errors_correctable', 'scale': 1},
    'XPUM_STATS_RAS_ERROR_CAT_DISPLAY_ERRORS_UNCORRECTABLE': {'name': 'xpum_display_errors_uncorrectable', 'scale': 1}
}


def get_metrics(core, pod_resources):

    try:
        code, _, data = core.getDeviceList()
        if code != 0:
            return '#NODATA'

        resp = b''

        for dev in data:
            stat_code, _, stat_data = core.getMetrics(dev.get('DeviceId'))

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
        metrics_name = metrics_type['name']
        scale = metrics_type['scale']
        value = stat.get('value')
        avg = stat.get('avg')
        if metrics_name not in metrics:
            if avg is not None:
                metrics[metrics_name] = Gauge(
                    metrics_name, f'{metrics_name}_DESCRIPTION', labelnames=labels, registry=registry)
                metrics[metrics_name].labels(*label_values).set(avg * scale)
            else:
                metrics[metrics_name] = Counter(
                    metrics_name, f'{metrics_name}_DESCRIPTION', labelnames=labels, registry=registry)
                metrics[metrics_name].labels(*label_values).inc(value * scale)

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
        labels.append('sub_dev')
        label_values.append(tile_id)


if __name__ == '__main__':
    # from DGMCore import DGMCore
    import stub as core
    from kube_pod_resource import get_pod_resources
    # core = DGMCore()
    pod_resources = get_pod_resources()
    metrics = get_metrics(core, pod_resources)
    print(metrics)
