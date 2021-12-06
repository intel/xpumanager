#pragma once
#include <string>
#include "level_zero/zes_api.h"

namespace xpum {

class device_process {
   private:
       uint32_t processId;
       uint64_t memSize;
       uint64_t sharedSize;
       zes_engine_type_flags_t engine;
   public:
       device_process(uint32_t processId, uint64_t memSize, uint64_t sharedSize, zes_engine_type_flags_t engine);
       ~device_process();
       uint32_t getProcessId();
       uint64_t getMemSize();
       uint64_t getSharedSize();
       zes_engine_type_flags_t getEngine();
};

} // end namespace xpum
