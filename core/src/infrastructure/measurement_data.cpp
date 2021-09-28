#include "measurement_data.h"

void MeasurementData::setSubdeviceDataCurrent(uint32_t subdevice_id, uint64_t data) {
    subdevice_datas[subdevice_id].current = data;
}

void MeasurementData::setSubdeviceDataMin(uint32_t subdevice_id, uint64_t data) {
    subdevice_datas[subdevice_id].min = data;
}

void MeasurementData::setSubdeviceDataMax(uint32_t subdevice_id, uint64_t data) {
    subdevice_datas[subdevice_id].max = data;
}

void MeasurementData::setSubdeviceDataAvg(uint32_t subdevice_id, uint64_t data) {
    subdevice_datas[subdevice_id].avg = data;
}

uint64_t MeasurementData::getSubdeviceDataCurrent(uint32_t subdevice_id) {
    if (subdevice_datas.find(subdevice_id) != subdevice_datas.end()) {
        return subdevice_datas[subdevice_id].current;
    }
    return -1;
}

uint64_t MeasurementData::getSubdeviceDataMin(uint32_t subdevice_id) {
    if (subdevice_datas.find(subdevice_id) != subdevice_datas.end()) {
        return subdevice_datas[subdevice_id].min;
    }
    return -1;
}

uint64_t MeasurementData::getSubdeviceDataMax(uint32_t subdevice_id) {
    if (subdevice_datas.find(subdevice_id) != subdevice_datas.end()) {
        return subdevice_datas[subdevice_id].max;
    }
    return -1;
}

uint64_t MeasurementData::getSubdeviceDataAvg(uint32_t subdevice_id) {
    if (subdevice_datas.find(subdevice_id) != subdevice_datas.end()) {
        return subdevice_datas[subdevice_id].avg;
    }
    return -1;
}

const std::map<uint32_t, SubdeviceData>& MeasurementData::getSubdeviceDatas() {
    return subdevice_datas;
}

uint32_t MeasurementData::getSubdeviceDataSize(){
    return subdevice_datas.size();
}

bool MeasurementData::hasSubdeviceData() {
    return subdevice_datas.size() >= 1;
}

uint64_t MeasurementData::getAdditionalDataCurrent(std::string name) {
    if (additional_datas.find(name) != additional_datas.end()) {
        return additional_datas[name].current;
    }
    return -1;
}

uint64_t MeasurementData::getAdditionalDataMax(std::string name) {
    if (additional_datas.find(name) != additional_datas.end()) {
        return additional_datas[name].max;
    }
    return -1;
}

uint64_t MeasurementData::getAdditionalDataMin(std::string name) {
    if (additional_datas.find(name) != additional_datas.end()) {
        return additional_datas[name].min;
    }
    return -1;
}

uint64_t MeasurementData::getAdditionalDataAvg(std::string name) {
    if (additional_datas.find(name) != additional_datas.end()) {
        return additional_datas[name].avg;
    }
    return -1;
}

uint64_t MeasurementData::getSubdeviceAdditionalDataCurrent(uint32_t subdevice_id, std::string name) {
    if (subdevice_additional_datas.find(subdevice_id) != subdevice_additional_datas.end()
            && subdevice_additional_datas[subdevice_id].find(name) != subdevice_additional_datas[subdevice_id].end()) {
        return subdevice_additional_datas[subdevice_id][name].current;
    }
    return -1;
}

uint64_t MeasurementData::getSubdeviceAdditionalDataMin(uint32_t subdevice_id, std::string name) {
    if (subdevice_additional_datas.find(subdevice_id) != subdevice_additional_datas.end()
            && subdevice_additional_datas[subdevice_id].find(name) != subdevice_additional_datas[subdevice_id].end()) {
        return subdevice_additional_datas[subdevice_id][name].min;
    }
    return -1;
}

uint64_t MeasurementData::getSubdeviceAdditionalDataMax(uint32_t subdevice_id, std::string name) {
    if (subdevice_additional_datas.find(subdevice_id) != subdevice_additional_datas.end()
            && subdevice_additional_datas[subdevice_id].find(name) != subdevice_additional_datas[subdevice_id].end()) {
        return subdevice_additional_datas[subdevice_id][name].max;
    }
    return -1;
}

uint64_t MeasurementData::getSubdeviceAdditionalDataAvg(uint32_t subdevice_id, std::string name) {
    if (subdevice_additional_datas.find(subdevice_id) != subdevice_additional_datas.end()
            && subdevice_additional_datas[subdevice_id].find(name) != subdevice_additional_datas[subdevice_id].end()) {
        return subdevice_additional_datas[subdevice_id][name].avg;
    }
    return -1;
}

void MeasurementData::setAdditionalDataCurrent(std::string name, uint64_t value) {
    additional_datas[name].current = value;
}

void MeasurementData::setAdditionalDataMax(std::string name, uint64_t value) {
    additional_datas[name].max = value;
}

void MeasurementData::setAdditionalDataMin(std::string name, uint64_t value) {
    additional_datas[name].min = value;
}

void MeasurementData::setAdditionalDataAvg(std::string name, uint64_t value) {
    additional_datas[name].avg = value;
}

void MeasurementData::setSubdeviceAdditionalDataCurrent(uint32_t subdevice_id, std::string name, uint64_t value) {
    subdevice_additional_datas[subdevice_id][name].current = value;
}

void MeasurementData::setSubdeviceAdditionalDataMin(uint32_t subdevice_id, std::string name, uint64_t value) {
    subdevice_additional_datas[subdevice_id][name].min = value;
}

void MeasurementData::setSubdeviceAdditionalDataMax(uint32_t subdevice_id, std::string name, uint64_t value) {
    subdevice_additional_datas[subdevice_id][name].max = value;
}

void MeasurementData::setSubdeviceAdditionalDataAvg(uint32_t subdevice_id, std::string name, uint64_t value) {
    subdevice_additional_datas[subdevice_id][name].avg = value;
}

bool MeasurementData::hasSubdeviceAdditionalData() {
    return subdevice_additional_datas.size() >= 1;
}