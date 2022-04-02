#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file kube_pod_resource.py
#

import grpc
import api_pb2_grpc as apigrpc
import api_pb2 as apipb2

from dev_file_converter import get_bdf_address

KUBELET_SOCKET = 'unix:///var/lib/kubelet/pod-resources/kubelet.sock'


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
    except grpc._channel._InactiveRpcError as e:
        print('grpc channel is inactive')
    except Exception as e:
        print('failed to get pod resource list due to: ', e)
    return ret


if __name__ == '__main__':
    print(get_pod_resources())
