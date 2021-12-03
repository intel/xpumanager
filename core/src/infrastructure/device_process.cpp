#include "device_process.h"

namespace xpum {
    device_process::device_process(uint32_t processId, uint64_t memSize, uint64_t sharedSize, zes_engine_type_flags_t engine){
        this->processId = processId;
        this->memSize = memSize;
        this->sharedSize = sharedSize;
        this->engine= engine;
    }
    device_process::~device_process() {}

    uint32_t device_process::getProcessId() {
        return this->processId;
    }

    uint64_t device_process::getMemSize() {
        return this->memSize;
    }

    uint64_t device_process::getSharedSize() {
        return this->sharedSize;
    }

    zes_engine_type_flags_t device_process::getEngine() {
        return this->engine;
    }

} // end namespace xpum
