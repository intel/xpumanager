import sys
import os

rest_folder = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

sys.path.insert(1, os.path.join(rest_folder,"prometheus_exporter"))


from prometheus_exporter import get_metrics
from kube_pod_resource import get_pod_resources

import stub

def export_metrics():
    return get_metrics(stub, get_pod_resources())