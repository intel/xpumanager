#pragma once

#include "xpum_structs.h"

#ifdef XPUM_EXPORTS
#define XPUM_API __declspec(dllexport)
#else
#define XPUM_API __declspec(dllimport)
#endif

namespace xpum {
extern "C" {
    XPUM_API xpum_result_t xpumGetSiblingDevices(xpum_device_id_t deviceId,
        xpum_device_id_t deviceList[], uint32_t *count);
}
} // end namespace xpum