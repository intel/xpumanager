
#include "igsc_err_msg.h"
#include "igsc_lib.h"

std::string print_device_fw_status(struct igsc_device_handle *handle) {
    auto status = igsc_get_last_firmware_status(handle);
    std::string msg = igsc_translate_firmware_status(status);
    return "Firmware status: " + msg + " (" + std::to_string(status) + ")";
}