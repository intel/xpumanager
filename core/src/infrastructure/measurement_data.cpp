#include "measurement_data.h"

namespace xpum {

void MeasurementData::setSubdeviceDataCurrent(uint32_t subdevice_id, uint64_t data) {
    (*p_subdevice_datas)[subdevice_id].current = data;
}

void MeasurementData::setSubdeviceDataRawTimestamp(uint32_t subdevice_id, uint64_t data) {
    (*p_subdevice_datas)[subdevice_id].raw_timestamp = data;
}

void MeasurementData::setSubdeviceRawData(uint32_t subdevice_id, uint64_t data) {
    (*p_subdevice_datas)[subdevice_id].raw_data = data;
}

void MeasurementData::setSubdeviceDataMin(uint32_t subdevice_id, uint64_t data) {
    (*p_subdevice_datas)[subdevice_id].min = data;
}

void MeasurementData::setSubdeviceDataMax(uint32_t subdevice_id, uint64_t data) {
    (*p_subdevice_datas)[subdevice_id].max = data;
}

void MeasurementData::setSubdeviceDataAvg(uint32_t subdevice_id, uint64_t data) {
    (*p_subdevice_datas)[subdevice_id].avg = data;
}

uint64_t MeasurementData::getSubdeviceDataCurrent(uint32_t subdevice_id) {
    if (p_subdevice_datas->find(subdevice_id) != p_subdevice_datas->end()) {
        return (*p_subdevice_datas)[subdevice_id].current;
    }
    return -1;
}

uint64_t MeasurementData::getSubdeviceDataMin(uint32_t subdevice_id) {
    if (p_subdevice_datas->find(subdevice_id) != p_subdevice_datas->end()) {
        return (*p_subdevice_datas)[subdevice_id].min;
    }
    return -1;
}

uint64_t MeasurementData::getSubdeviceDataMax(uint32_t subdevice_id) {
    if (p_subdevice_datas->find(subdevice_id) != p_subdevice_datas->end()) {
        return (*p_subdevice_datas)[subdevice_id].max;
    }
    return -1;
}

uint64_t MeasurementData::getSubdeviceDataAvg(uint32_t subdevice_id) {
    if (p_subdevice_datas->find(subdevice_id) != p_subdevice_datas->end()) {
        return (*p_subdevice_datas)[subdevice_id].avg;
    }
    return -1;
}

uint64_t MeasurementData::getSubdeviceDataRawTimestamp(uint32_t subdevice_id) {
    if (p_subdevice_datas->find(subdevice_id) != p_subdevice_datas->end()) {
        return (*p_subdevice_datas)[subdevice_id].raw_timestamp;
    }
    return -1;
}

uint64_t MeasurementData::getSubdeviceRawData(uint32_t subdevice_id) {
    if (p_subdevice_datas->find(subdevice_id) != p_subdevice_datas->end()) {
        return (*p_subdevice_datas)[subdevice_id].raw_data;
    }
    return -1;
}

const std::shared_ptr<std::map<uint32_t, SubdeviceData>> MeasurementData::getSubdeviceDatas() {
    return p_subdevice_datas;
}

uint32_t MeasurementData::getSubdeviceDataSize() {
    return p_subdevice_datas->size();
}

bool MeasurementData::hasSubdeviceData() {
    return p_subdevice_datas->size() >= 1;
}
} // end namespace xpum
