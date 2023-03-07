/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file load_igsc.h
 */

#pragma once

#include <dlfcn.h>
#include <igsc_lib.h>
#include <link.h>

#include <cstddef>

namespace xpum {

struct igsc_psc_version {
    uint32_t date;        /**< PSC date */
    uint32_t cfg_version; /**< PSC configuration version */
};

typedef int (*igsc_device_psc_version_t)(struct igsc_device_handle *handle, struct igsc_psc_version *version);

class LibIgsc {
   private:
    void *handle;

   public:
    igsc_device_psc_version_t igsc_device_psc_version;
    LibIgsc() {
        handle = dlopen("libigsc.so.0", RTLD_LAZY);
        if (!handle)
            return;
        igsc_device_psc_version = reinterpret_cast<igsc_device_psc_version_t>(dlsym(handle, "igsc_device_psc_version"));
    }

    bool ok() {
        return handle != NULL && igsc_device_psc_version != NULL;
    }
};
} // namespace xpum