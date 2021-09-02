from prometheus_client import CollectorRegistry, Gauge, generate_latest

import traceback


def get_metrics(core, pod_resources):

    try:
        registry = CollectorRegistry()
        labels = ['uuid', 'dev_name', 'pci_dev_id',
                  'sub_dev_id', 'vendor', 'pci_bdf', 'kube_pod', 'kube_namespace', 'kube_container']

        code, _, data = core.getDeviceList()
        metrics = {}
        if code == 0:

            for dev in data:
                stat_code, _, stat_data = core.getStatistics(
                    dev.get('DeviceId'))
                if stat_code == 0 and 'dataList' in stat_data:
                    label_values = [
                        dev.get('UUID', ''),
                        dev.get('DeviceName', ''),
                        dev.get('PCIDeviceId', ''),
                        dev.get('SubDeviceId', ''),
                        dev.get('VendorName', ''),
                        dev.get('PCIBDFAddress', '')
                    ]

                    attach_pod_labels(dev, label_values, pod_resources)

                    for stat in stat_data['dataList']:
                        metrics_type = stat.get('metricsType')
                        value = stat.get('value')
                        avg = stat.get('avg')
                        export_value = avg if avg is not None else value
                        if export_value is not None:
                            if metrics_type not in metrics:
                                metrics[metrics_type] = Gauge(metrics_type, f'{metrics_type}_DESCRIPTION',
                                                              labelnames=labels, registry=registry)
                            gauge = metrics[metrics_type]
                            gauge.labels(*label_values).set(export_value)

        return generate_latest(registry)
    except:
        traceback.print_exc()


def attach_pod_labels(dev, label_values, pod_resources):
    bdf = dev.get('PCIBDFAddress', '')
    pod_resource = pod_resources.get(bdf, {})
    label_values.extend([
        pod_resource.get('pod', ''),
        pod_resource.get('namespace', ''),
        pod_resource.get('container', '')
    ])


if __name__ == '__main__':
    from DGMCore import DGMCore
    from kube_pod_resource import get_pod_resources
    core = DGMCore()
    pod_resources = get_pod_resources()
    metrics = get_metrics(core, pod_resources)
    print(metrics)
