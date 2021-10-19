from google.protobuf import empty_pb2
from .grpc_stub import stub


def getVersion():
    resp = stub.getVersion(empty_pb2.Empty())
    if len(resp.errorMsg) != 0:
        print("error msg " + resp.errorMsg)
        return 1, "Fail to get version info", None
    data = dict()
    for version in resp.versions:
        # print( version.versionString )
        versionStr = version.versionString
        versionType = version.version
        if versionType == 0:
            data['Version'] = versionStr
        elif versionType == 1:
            data['LevelZeroVersion'] = versionStr
    return 0, "OK", data
