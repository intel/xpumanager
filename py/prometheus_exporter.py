from prometheus_client import CollectorRegistry, Gauge, Counter, generate_latest

import os
import traceback


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

        return resp
    except Exception as e:
        print(e)
        traceback.print_exc()


def convert_to_prometheus_metrics(pod_resources, dev, datalist, tile_id=None):

    labels, label_values = build_basic_labels(dev)
    attach_kube_labels(dev, labels, label_values, pod_resources)
    attach_tile_labels(labels, label_values, tile_id)

    registry = CollectorRegistry()
    metrics = {}

    for stat in datalist:
        metrics_type: str = stat.get('metricsType')
        metrics_type = 'xpum_' + metrics_type[11:].lower()
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
    labels = ['uuid', 'dev_name', 'pci_dev_id',
              'sub_dev_id', 'vendor', 'pci_bdf']
    label_values = [dev.get(key, '') for key in [
        'UUID', 'DeviceName', 'PCIDeviceId', 'SubDeviceId', 'VendorName', 'PCIBDFAddress']]

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
