/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_sensor.cpp
 */

#include "comlet_sensor.h"
#include "cli_table.h"

using namespace nlohmann;

namespace xpum::cli {

static CharTableConfig ComletSensorTableConfig(R"({
    "columns": [
        {
            "title": "Sensor Name"
        },
        {
            "title": "Value"
        },
        {
            "title": "Low"
        },
        {
            "title": "High"
        },
        {
            "title": "Unit"
        }
    ],
    "rows": [
        {
            "instance": "sensor_reading[]",
            "cells": [
                "sensor_name",
                "value",
                "sensor_low",
                "sensor_high",
                "sensor_unit"
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
        out << "No sensor found" << std::endl;
        return;
    }
    std::map<int, json> m;
    for (auto obj : sensorJson["sensor_reading"]) {
        int amc_index = obj["amc_index"];
        m[amc_index].push_back(obj);
    }
    for (auto it = m.begin(); it != m.end(); it++) {
        int amc_index = it->first;
        auto sensor_reading = it->second;
        json obj;
        obj["amc_index"] = amc_index;
        obj["sensor_reading"] = sensor_reading;

        out << "AMC " << amc_index << std::endl;
        CharTable table(ComletSensorTableConfig, obj);
        table.show(out);
    }
}
}