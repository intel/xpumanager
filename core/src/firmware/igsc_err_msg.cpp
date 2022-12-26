
#include "igsc_err_msg.h"

#define IGSC_ERROR_BASE 0x0000U

std::string transIgscErrCodeToMsg(int code) {
    switch (code) {
        case IGSC_ERROR_BASE + 0:
            return "Success";
        case IGSC_ERROR_BASE + 1:
            return "Internal Error";
        case IGSC_ERROR_BASE + 2:
            return "Memory Allocation Failed";
        case IGSC_ERROR_BASE + 3:
            return "Invalid parameter was provided";
        case IGSC_ERROR_BASE + 4:
            return "Requested device was not found";
        case IGSC_ERROR_BASE + 5:
            return "Provided image has wrong format";
        case IGSC_ERROR_BASE + 6:
            return "Error in the update protocol";
        case IGSC_ERROR_BASE + 7:
            return "Provided buffer is too small";
        case IGSC_ERROR_BASE + 8:
            return "Invalid library internal state";
        case IGSC_ERROR_BASE + 9:
            return "Unsupported request";
        case IGSC_ERROR_BASE + 10:
            return "Incompatible request";
        case IGSC_ERROR_BASE + 11:
            return "The operation has timed out";
        case IGSC_ERROR_BASE + 12:
            return "The process doesn't have access rights";
        default:
            return "Unknown error";
    }
}