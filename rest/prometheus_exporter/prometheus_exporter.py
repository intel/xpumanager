#
# Copyright (C) 2021-2023 Intel Corporation
# SPDX-License-Identifier: MIT
# @file prometheus_exporter.py
#

from prometheus_client import CollectorRegistry, Gauge, Counter, generate_latest

import os
import traceback

from prometheus_exporter_types import device_to_card_aggregators, tile_to_device_aggregators, metrics_map
from itertools import filterfalse, groupby

import xpum_logger as logger

registries = {}
counter_values = {}


def get_metrics(core, pod_resources):

    try:
        # processing device metrics
        code, _, devices = core.getDeviceList()
        if code != 0:
            return f'#nodata: failed to get devices ({code})', 500

        resp_devices, all_device_data = process_device_stats(
            core, pod_resources, devices)
        resp_cards = process_card_stats(core, pod_resources, all_device_data)
        resp_per_engine = process_per_engine_stats(
            core, pod_resources, devices)
        resp_fabric_throughput = process_fabric_stats(
            core, pod_resources, devices)

        resp_topology_link = process_topology_link(
            core, pod_resources, devices)

        resp_xelink_port_status = process_xelink_port_stats(
            core, pod_resources, devices)

        return tidy_response(resp_devices + resp_cards + resp_per_engine + resp_fabric_throughput + resp_topology_link + resp_xelink_port_status)
    except Exception as e:
        traceback.print_exc()
        return "#nodata: due to unexpected failure", 500

def process_xelink_port_stats(core, pod_resources, devices):

    resp = b''

    for dev in devices:

        device_id = dev.get('device_id')

        stat_code, _, stat_data = core.getXelinkPortHealth(
            device_id, session_id=1)

        if stat_code != 0:
            continue

        data_list = []

        stat_data['metrics_type'] = 'XPUM_STATS_XELINK_PORT_STATUS'
        data_list.append(stat_data)

        r = convert_to_prometheus_metrics(
            pod_resources, dev, data_list, device_id, None)

        resp = resp + r

    return resp

def process_topology_link(core, pod_resources, devices):

    resp = b''

    stat_code, _, stat_data = core.getTopologyLink()

    if stat_code != 0 or 'topology_link' not in stat_data:
        return resp

    for dev in devices:

        device_id = dev.get('device_id')
        data_list = []

        for link in stat_data['topology_link']:
            if link.get('local_device_id') != device_id:
                continue
            link['metrics_type'] = 'XPUM_STATS_TOPOLOGY_LINK'
            data_list.append(link)

        r = convert_to_prometheus_metrics(
            pod_resources, dev, data_list, device_id, None)

        resp = resp + r

    return resp

def process_fabric_stats(core, pod_resources, devices):

    resp = b''

    for dev in devices:

        device_id = dev.get('device_id')

        stat_code, _, stat_data = core.getFabricStatistics(
            device_id, session_id=1, get_accumulated=True)

        if stat_code != 0 or 'fabric_throughput' not in stat_data:
            continue

        data_list = []
        throughput_data_list = []

        for link in stat_data['fabric_throughput']:
            # type 3 is XPUM_FABRIC_THROUGHPUT_TYPE_TRANSMITTED_COUNTER
            if link.get('src_device_id') == device_id and link.get('type') == 3:

                link['metrics_type'] = 'XPUM_STATS_FABRIC_THROUGHPUT'
                dst_device = next((x for x in devices if x.get(
                    'device_id') == link.get('dst_device_id')), None)
                if dst_device is None:
                    logger.warn(
                        'Cannot find information for fabric link destination device %s', device_id)
                    continue

                link['dst_pci_bdf'] = dst_device.get('pci_bdf_address', 'N/A')
                link['dst_dev_file'] = get_dev_shortname(dst_device.get('drm_device'))
                data_list.append(link)
            elif link.get('src_device_id') == device_id and (link.get('type') == 0 or link.get('type') == 1):
                link['metrics_type'] = 'XPUM_STATS_XELINK_THROUGHPUT'
                link['local_device_id'] = link['src_device_id']
                link['local_subdevice_id'] = link['src_tile_id']
                link['remote_device_id'] = link['dst_device_id']
                link['remote_subdevice_id'] = link['dst_tile_id']
                throughput_data_list.append(link)

        r = convert_to_prometheus_metrics(
            pod_resources, dev, data_list, device_id, None)

        s = convert_to_prometheus_metrics(
            pod_resources, dev, throughput_data_list, device_id, None)

        resp = resp + r + s

    return resp


def process_per_engine_stats(core, pod_resources, devices):

    resp = b''

    for dev in devices:

        device_id = dev.get('device_id')

        stat_code, _, stat_data = core.getEngineStatistics(
            device_id, session_id=1)

        if stat_code != 0 or 'engine_util' not in stat_data:
            continue

        for tile_id, data_map in stat_data['engine_util'].items():
            flatten_per_engine_datalist = flatten_per_engine_data(data_map)
            r = convert_to_prometheus_metrics(
                pod_resources, dev, flatten_per_engine_datalist, device_id, None if tile_id == 'device_level' else tile_id)
            resp = resp + r

    return resp


def flatten_per_engine_data(data_map):
    data_list = []
    for engine_type, datas in data_map.items():
        for data in datas:
            data['metrics_type'] = 'XPUM_STATS_ENGINE_UTILIZATION'
            data['engine_type'] = engine_type
            data_list.append(data)
    return data_list


def process_device_stats(core, pod_resources, devices):

    resp = b''
    all_device_data = {}

    for dev in devices:

        device_id = dev.get('device_id')

        stat_code, _, stat_data = core.getStatistics(
            device_id, session_id=1, get_accumulated=True)

        if stat_code != 0:
            continue

            # aggregate tile metrics so that they will be exported at device level
        aggregate_tile_to_device(stat_data)

        if 'device_level' in stat_data:

            # store all device data in dict for later card level aggregation
            all_device_data[device_id] = stat_data['device_level']

            # export device metrics to Prometheus registry
            r = convert_to_prometheus_metrics(
                pod_resources, dev, stat_data['device_level'], device_id)
            resp = resp + r

        # export tile metrics to Prometheus registry
        for tile_data in stat_data.get('tile_level', []):
            r = convert_to_prometheus_metrics(
                pod_resources, dev, tile_data['data_list'], device_id, tile_data['tile_id'])
            resp = resp + r

    return resp, all_device_data


def process_card_stats(core, pod_resources, all_device_data):
    resp = b''
    all_card_data = aggregate_device_to_card(core, all_device_data)
    for card_id, card_data in all_card_data.items():
        r = convert_to_prometheus_metrics(
            pod_resources, dev=None, datalist=card_data, card_id=card_id)
        resp = resp + r
    return resp


def aggregate_device_to_card(core, all_device_data):

    INVALID_GROUP_ID = 0
    device_to_card_map = {}
    # construct mapping "device->card"
    code, _, data = core.getAllGroups()
    if code != 0:
        return f'#nodata: failed to get cards ({code})', 500

    for group in data:
        group_id = group.get('group_id')
        # only process built-in groups (i.e., cards) whose group id has 1 as the highest bit
        if group_id is None or group_id & 0x80000000 != 0x80000000:
            continue

        for device_id in group.get('device_id_list', []):
            # a device should belong to one card at most, no check here
            device_to_card_map[device_id] = group_id

    # group all device data by card
    groups_data = [(device_to_card_map.get(x, INVALID_GROUP_ID),
                    all_device_data[x]) for x in all_device_data]

    def key_func(x): return x[0]
    groups_data = sorted(groups_data, key=key_func)
    groups_data = groupby(groups_data, key_func)
    def filter_func(x): return x[0] == INVALID_GROUP_ID
    valid_groups_data = filterfalse(filter_func, groups_data)

    all_card_data = {}
    for card_id, data_list in valid_groups_data:
        # group card data by metric type
        group_data = [x[1] for x in list(data_list)]
        group_data = [item for sublist in group_data for item in sublist]
        def key_func(x): return x['metrics_type']
        group_data_by_type = groupby(
            sorted(group_data, key=key_func), key_func)

        def filter_func(x): return x[0] not in device_to_card_aggregators
        group_data_by_type = filterfalse(filter_func, group_data_by_type)

        # aggregate device data to card
        aggregate_data(group_data_by_type, device_to_card_aggregators,
                       all_card_data.setdefault(card_id, []))

    return all_card_data


def aggregate_tile_to_device(stat_data):

    device_level = stat_data.get('device_level', {})
    device_metrics = [x['metrics_type'] for x in device_level]
    # group tile data by metrics_type
    tile_level = [x['data_list'] for x in stat_data.get('tile_level', [])]
    # groupby needs sorted list by the same key_func
    def key_func(x): return x['metrics_type']
    tile_level = sorted(
        [item for sublist in tile_level for item in sublist], key=key_func)
    tiles_data_by_type = groupby(tile_level, key_func)
    # device_level already has this metric, skip aggregation from tile
    def filter_func(
        x): return x[0] not in tile_to_device_aggregators or x[0] in device_metrics
    tiles_data_by_type = filterfalse(filter_func, tiles_data_by_type)

    # aggregate tile data to device
    aggregate_data(tiles_data_by_type,
                   tile_to_device_aggregators, device_level)

    stat_data['device_level'] = device_level


def aggregate_data(data_by_type, data_aggregators, target_list):
    for metric_type, data in data_by_type:
        data = list(data)
        device_data = {'metrics_type': metric_type}
        for value_type in data[0]:
            if value_type in data_aggregators[metric_type]:
                agg = tile_to_device_aggregators[metric_type][value_type]
                value_list = [x[value_type] for x in data]
                agg_value = agg(value_list)
                device_data[value_type] = agg_value
                device_data['agg_func'] = agg.__name__
        target_list.append(device_data)


def tidy_response(resp):
    resp_str = resp.decode('UTF-8')
    comments = []
    metrics = []
    for line in resp_str.splitlines():
        if not line.startswith('#'):
            if line not in metrics:
                metrics.append(line)
        elif line not in comments:
            comments.append(line)
    metrics.sort()
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
        metrics_list = metrics_map.get(stat.get('metrics_type'))
        if metrics_list is None:
            continue

        if type(metrics_list) is not list:
            metrics_list = [metrics_list]

        for metric in metrics_list:
            metric_name = metric.prom_metric.name

            val = stat.get(metric.xpum_field) * metric.scale

            all_labelnames, all_labelvalues, ext_labelvalues = attach_ext_labels(
                labels, label_values, metric.prom_metric.ext_labelnames, metric.ext_labels, stat)

            attach_src_label(all_labelnames, all_labelvalues,
                             stat.get('agg_func', 'direct'))

            counter_key = f'{metrics_owner}_{metric_name}_{ext_labelvalues}'

            if metric_name not in metrics:
                if metric.is_counter:
                    # counter value
                    metrics[metric_name] = Counter(
                        metric_name, metric.prom_metric.desc, labelnames=all_labelnames, registry=registry)
                    mm = metrics[metric_name].labels(*all_labelvalues)
                    counter_values[counter_key] = val
                    mm.inc(val)
                else:
                    # aggregated value
                    metrics[metric_name] = Gauge(
                        metric_name, metric.prom_metric.desc, labelnames=all_labelnames, registry=registry)
                    metrics[metric_name].labels(*all_labelvalues).set(val)
            else:
                if metric.is_counter:
                    # counter value
                    mm = metrics[metric_name].labels(*all_labelvalues)
                    pre_value = counter_values.setdefault(counter_key, 0)
                    if val >= pre_value:
                        counter_values[counter_key] = val
                        mm.inc(val-pre_value)
                    elif metric.process_counter_wrap:
                        logger.info(
                            'counter wrapped, %s: pre=%d, cur=%d, create new counter', metric_name, pre_value, val)
                        # remove the existing metric child
                        metrics[metric_name].remove(*all_labelvalues)
                        del counter_values[counter_key]
                        # create new metric child
                        mm = metrics[metric_name].labels(*all_labelvalues)
                        counter_values[counter_key] = val
                        mm.inc(val)
                    else:
                        logger.warn(
                            'counter decreased, %s: pre=%d, cur=%d, ignore it', metric_name, pre_value, val)
                else:
                    # aggregated value
                    metrics[metric_name].labels(*all_labelvalues).set(val)

    return generate_latest(registry)


def build_dev_labels(dev):
    if dev is None:
        return [], []

    labels = ['uuid', 'dev_name', 'pci_dev', 'vendor', 'pci_bdf']
    label_values = [dev.get(key, '') for key in [
        'uuid', 'device_name', 'pci_device_id', 'vendor_name', 'pci_bdf_address']]

    dev_file = get_dev_shortname(dev.get('drm_device'))
    if dev_file is not None:
        labels.append('dev_file')
        label_values.append(dev_file)

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


def attach_src_label(labels, label_values, src):
    labels.append('src')
    label_values.append(src)


def attach_ext_labels(labels, label_values, ext_labelnames, ext_labels, stat=None):
    if ext_labelnames is not None and len(ext_labelnames) > 0:
        all_labelnames = labels + ext_labelnames

        ext_labelvalues = []
        for key in ext_labelnames:
            value: str = ext_labels.get(key, 'n/a')
            if value.startswith('$') and stat is not None:
                value = stat.get(value[1:], 'n/a')
            ext_labelvalues.append(value)
        all_labelvalues = label_values + ext_labelvalues
        return all_labelnames, all_labelvalues, str(ext_labelvalues)
    return labels.copy(), label_values.copy(), ''

def get_dev_shortname(fullname: str):
    if fullname is None:
        return None
    return fullname.split('/')[-1]