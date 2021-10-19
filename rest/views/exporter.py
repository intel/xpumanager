from prometheus_exporter.prometheus_exporter import get_metrics
from prometheus_exporter.kube_pod_resource import get_pod_resources
import stub

def export_metrics():
    return get_metrics(stub, get_pod_resources())