/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_sensor.cpp
 */

#include "comlet_sensor.h"
#include "cli_table.h"

namespace xpum::cli {

static CharTableConfig ComletSensorTableConfig(R"({
    "columns": [{
        "title": "AMC Index"
    },{
        "title": "Sensor Name"
    },{
        "title": "Value"
    },{
        "title": "Low"
    },{
        "title": "High"
    },{
        "title": "Unit"
    }],
    "rows": [{
        "instance": "sensor_reading[]",
        "cells": [
            { "value": "amc_index" },
            "sensor_name", 
            "value", 
            "sensor_low",
            "sensor_high",
            "sensor_unit"
        ]
    }]
})"_json);

std::unique_ptr<nlohmann::json> ComletSensor::run(){
    return this->coreStub->getSensorReading();
}

void ComletSensor::getTableResult(std::ostream &out) {
    auto p_json = run();
    CharTable table(ComletSensorTableConfig, *p_json);
    table.show(out);
}
}