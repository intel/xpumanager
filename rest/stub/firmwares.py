from .grpc_stub import stub
import core_pb2
from .xpum_enums import XpumResult


def runFirmwareFlash(deviceId, firmwareType, filePath):
    job = core_pb2.XpumFirmwareFlashJob()
    job.id.id = deviceId
    job.type.value = firmwareType
    job.path = filePath
    resp = stub.runFirmwareFlash(job)
    if resp.value == XpumResult['XPUM_UPDATE_FIRMWARE_UNSUPPORTED'].value:
        return 1, "AMC Firmware update unsupported; AMC firmware update just works for one ATS-P card (AMC firmware version is 3.3 or newer) on Intel M50CYP server (BMC firmware version is 2.82 or newer) so far", None
    elif resp.value != XpumResult['XPUM_OK'].value:
        return 1, "Fail to run firmware flash", None
    else:
        data = dict()
        data['run_firmware_flash_result'] = 'OK'
        return 0, "OK", data


def getFirmwareFlashResult(deviceId, firmwareType):
    request = core_pb2.XpumFirmwareFlashTaskRequest()
    request.id.id = deviceId
    request.type.value = firmwareType
    resp = stub.getFirmwareFlashResult(request)

    if len(resp.errorMsg) != 0:
        return 1, "Fail to get firmware flash result", None
    else:
        data = dict()
        data['device_id'] = resp.id.id
        if resp.type.value == 0:
            data['device_type'] = 'GSC'
        else:
            data['device_type'] = 'AMC'

        if resp.result.value == 0:
            data['firmware_flash_result'] = 'OK'
        elif resp.result.value == 1:
            data['firmware_flash_result'] = 'FAILED'
        else:
            data['firmware_flash_result'] = 'ONGOING'

        return 0, "OK", data
