from .grpc_stub import stub
import core_pb2
from google.protobuf import empty_pb2

def getTopology(deviceId):
  resp = stub.getTopology(core_pb2.DeviceId(id=deviceId))
  if len(resp.errorMsg) != 0:
    return 1, resp.errorMsg, None
  data = dict()
  data["device_id"] = deviceId
  data["affinity_localcpulist"] = resp.cpuAffinity.localCpuList
  data["affinity_localcpus"]  = resp.cpuAffinity.localCpus
  data["switch_count"] = resp.switchCount
  if resp.switchCount > 0:
    data["switch_list"] = [s.switchDevicePath for s in resp.switchInfo]
  return 0, "OK", data


def exportTopology():
  resp = stub.getTopoXMLBuffer()
  if len(resp.errorMsg) != 0:
    return 1, resp.errorMsg, None
  
  return 0, "OK", resp.xmlstring