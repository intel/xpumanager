import re
import os
import grpc
from . import api_pb2_grpc as apigrpc
from . import api_pb2 as apipb2

import traceback

KUBELET_SOCKET = 'unix:///var/lib/kubelet/pod-resources/kubelet.sock'

device_id_pattern = re.compile('(card\d+)\-\d*')
bdf_pattern = re.compile(
    '.*/([0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-9a-fA-F]{1}\S*)')


def get_pod_resources():

    ret = {}
    try:
        with grpc.insecure_channel(KUBELET_SOCKET) as channel:

            pod_resource_lister = apigrpc.PodResourcesListerStub(channel)
            res = pod_resource_lister.List(apipb2.ListPodResourcesRequest())

            for pod_resource in res.pod_resources:
                for container in pod_resource.containers:
                    for device in container.devices:
                        if device.resource_name == 'gpu.intel.com/i915':
                            for device_id in device.device_ids:
                                bdf = get_bdf_address(device_id)
                                if bdf is not None:
                                    ret[bdf] = {
                                        'pod': pod_resource.name,
                                        'namespace': pod_resource.namespace,
                                        'container': container.name,
                                        'device_id': device_id
                                    }
    except:
        traceback.print_exc()
    return ret


def get_bdf_address(device_id):
    device_id_match = device_id_pattern.fullmatch(device_id)
    if device_id_match is not None:
        card_dev = device_id_match.group(1)
        link = os.readlink(f'/sys/class/drm/{card_dev}/device')
        bdf_match = bdf_pattern.fullmatch(link)
        if bdf_match is not None:
            return bdf_match.group(1)

    return None


if __name__ == '__main__':
    print(get_pod_resources())
