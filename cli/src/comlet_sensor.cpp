/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_sensor.cpp
 */

#include "comlet_sensor.h"
#include "cli_table.h"
#include <iomanip>
#include <sstream>

using namespace nlohmann;

namespace xpum::cli {

static CharTableConfig ComletSensorTableConfig(R"({
    "columns": [{
        "title": "AMC ID"
    }, {
        "title": "Sensors"
    }],
    "rows": [
        {
            "instance": "amc_sensor_reading_list[]",
            "cells": [
                "amc_index",
                "value"
            ]
        }
    ]
})"_json);

std::unique_ptr<nlohmann::json> ComletSensor::run(){
    return this->coreStub->getSensorReading();
}

void ComletSensor::getTableResult(std::ostream &out) {
    auto p_json = run();
    json sensorJson = (*p_json);
    if (sensorJson["sensor_reading"].size() == 0) {
        out << "No AMC sensor found" << std::endl;
        return;
    }
    std::map<int, json> m;
    for (auto obj : sensorJson["sensor_reading"]) {
        int amc_index = obj["amc_index"];
        m[amc_index].push_back(obj);
    }
    json amc_sensor_reading_list;
    for (auto it = m.begin(); it != m.end(); it++) {
        int amc_index = it->first;
        auto sensor_reading = it->second;
        std::string amc_sensor_values;
        json obj;
        obj["amc_index"] = "AMC " + std::to_string(amc_index);
        for(auto reading:sensor_reading){
            std::stringstream ss;
            ss << std::endl;
            ss << reading["sensor_name"].get<std::string>();
            ss << " (";
            ss << reading["sensor_unit"].get<std::string>();
            ss << "): ";
            double value = reading["value"];
            if (value == (int)value) {
                ss << (int)value;
            } else {
                ss << std::fixed << std::setprecision(3) << value;
            }
            amc_sensor_values+=ss.str();
        }
        
        obj["value"] = amc_sensor_values;
        amc_sensor_reading_list.push_back(obj);
    }
    json json4table;
    json4table["amc_sensor_reading_list"] = amc_sensor_reading_list;
    CharTable table(ComletSensorTableConfig, json4table);
    table.show(out);
}
}
