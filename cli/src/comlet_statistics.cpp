/* 
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_statistics.cpp
 */

#include "comlet_statistics.h"

#include <map>
#include <sstream>
#include <nlohmann/json.hpp>

#include "cli_table.h"
#include "core_stub.h"
#include "utility.h"
#include "exit_code.h"

namespace xpum::cli {

#ifndef DAEMONLESS
static CharTableConfig ComletConfigDeviceStatistics(R"({
    "showTitleRow": false,
    "columns": [{
        "title": "none",
        "size": 26
    }, {
        "title": "none"
    }],
    "rows": [{
        "instance": "",
        "cells": [
            { "rowTitle": "Device ID" },
            "device_id"
        ]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Start Time" },
            { "rowTitle": "End Time" },
            { "rowTitle": "Elapsed Time (Second) " },
            { "rowTitle": "Energy Consumed (J) " },
            { "rowTitle": "Average % utilization of all GPU Engines " },
            { "rowTitle": "Compute Engines Util (%) " },
            { "rowTitle": "Render Engines Util (%) " },
            { "rowTitle": "Media Engines Util (%) " },
            { "rowTitle": "Copy Engines Util (%) " },
            { "rowTitle": "EU Array Active (%) " },
            { "rowTitle": "EU Array Stall (%) " },
            { "rowTitle": "EU Array Idle (%) " }
        ], [
            { "value": "begin" },
            { "value": "end" },
            { "value": "elapsed_time" },
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENERGY].value", "scale": 1 }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_UTILIZATION].avg", "fixer": "round" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENGINE_GROUP_COMPUTE_ALL_UTILIZATION].avg", "fixer": "round" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENGINE_GROUP_RENDER_ALL_UTILIZATION].avg", "fixer": "round" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION].avg", "fixer": "round" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENGINE_GROUP_COPY_ALL_UTILIZATION].avg", "fixer": "round" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_ACTIVE].avg", "scale": 1 }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_STALL].avg", "scale": 1 }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_IDLE].avg", "scale": 1 }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Reset" },
            { "rowTitle": "Programming Errors" },
            { "rowTitle": "Driver Errors" },
            { "rowTitle": "Cache Errors Correctable" },
            { "rowTitle": "Cache Errors Uncorrectable" },
            { "rowTitle": "Mem Errors Correctable" },
            { "rowTitle": "Mem Errors Uncorrectable" }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_RESET].value" },
                { "label": "total", "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_RESET].total" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS].value" },
                { "label": "total", "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS].total" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS].value" },
                { "label": "total", "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS].total" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE].value" },
                { "label": "total", "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE].total" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE].value" },
                { "label": "total", "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE].total" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE].value" },
                { "label": "total", "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE].total" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE].value" },
                { "label": "total", "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE].total" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU Power (W) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_POWER].avg", "fixer": "round" },
                { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_POWER].min", "fixer": "round" },
                { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_POWER].max", "fixer": "round" },
                { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_POWER].value", "fixer": "round" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU Frequency (MHz) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_GPU_FREQUENCY].avg" },
                { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_GPU_FREQUENCY].min" },
                { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_GPU_FREQUENCY].max" },
                { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_GPU_FREQUENCY].value" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Media Engine Freq (MHz) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEDIA_ENGINE_FREQUENCY].avg" },
                { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEDIA_ENGINE_FREQUENCY].min" },
                { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEDIA_ENGINE_FREQUENCY].max" },
                { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEDIA_ENGINE_FREQUENCY].value" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU Core Temperature\n(Celsius Degree) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_GPU_CORE_TEMPERATURE].avg", "fixer": "round" },
                { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_GPU_CORE_TEMPERATURE].min", "fixer": "round" },
                { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_GPU_CORE_TEMPERATURE].max", "fixer": "round" },
                { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_GPU_CORE_TEMPERATURE].value", "fixer": "round" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU Memory Temperature\n(Celsius Degree) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_TEMPERATURE].avg", "fixer": "round" },
                { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_TEMPERATURE].min", "fixer": "round" },
                { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_TEMPERATURE].max", "fixer": "round" },
                { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_TEMPERATURE].value", "fixer": "round" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU Memory Read (kB/s) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].avg", "fixer": "round" },
                { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].min", "fixer": "round" },
                { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].max", "fixer": "round" },
                { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].value", "fixer": "round" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU Memory Write (kB/s) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].avg", "fixer": "round" },
                { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].min", "fixer": "round" },
                { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].max", "fixer": "round" },
                { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].value", "fixer": "round" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU Memory Bandwidth (%) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_BANDWIDTH].avg" },
                { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_BANDWIDTH].min" },
                { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_BANDWIDTH].max" },
                { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_BANDWIDTH].value" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU Memory Used (MiB) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].avg", "scale": 1, "fixer": "round" },
                { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].min", "scale": 1, "fixer": "round" },
                { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].max", "scale": 1, "fixer": "round" },
                { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].value", "scale": 1, "fixer": "round" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU Memory Util (%) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_UTILIZATION].avg", "fixer": "round" },
                { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_UTILIZATION].min", "fixer": "round" },
                { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_UTILIZATION].max", "fixer": "round" },
                { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_UTILIZATION].value", "fixer": "round" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "PCIe Read (kB/s) " }
        ], [
            { "value": "", "subs": [
                { "label": "avg", "value": "device_level[metrics_type==XPUM_STATS_PCIE_READ_THROUGHPUT].avg" },
                { "label": "min", "value": "device_level[metrics_type==XPUM_STATS_PCIE_READ_THROUGHPUT].min" },
                { "label": "max", "value": "device_level[metrics_type==XPUM_STATS_PCIE_READ_THROUGHPUT].max" },
                { "label": "current", "value": "device_level[metrics_type==XPUM_STATS_PCIE_READ_THROUGHPUT].value" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "PCIe Write (kB/s) " }
        ], [
            { "value": "", "subs": [
                { "label": "avg", "value": "device_level[metrics_type==XPUM_STATS_PCIE_WRITE_THROUGHPUT].avg" },
                { "label": "min", "value": "device_level[metrics_type==XPUM_STATS_PCIE_WRITE_THROUGHPUT].min" },
                { "label": "max", "value": "device_level[metrics_type==XPUM_STATS_PCIE_WRITE_THROUGHPUT].max" },
                { "label": "current", "value": "device_level[metrics_type==XPUM_STATS_PCIE_WRITE_THROUGHPUT].value" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Compute Engine Util (%) " }
        ], [
            { "value": "compute_engine_util"}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Render Engine Util (%) " }
        ], [
            { "value": "render_engine_util"}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Decoder Engine Util (%) " }
        ], [
            { "value": "decoder_engine_util"}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Encoder Engine Util (%) " }
        ], [
            { "value": "encoder_engine_util"}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Copy Engine Util (%) " }
        ], [
            { "value": "copy_engine_util"}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Media EM Engine Util (%) " }
        ], [
            { "value": "media_em_engine_util"}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "3D Engine Util (%) " }
        ], [
            { "value": "3d_engine_util"}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Xe Link Throughput (kB/s) " }
        ], [
            { "value": "fabric_throughput"}
        ]]
    }]
})"_json);
#else
static CharTableConfig ComletConfigDeviceStatistics(R"({
    "showTitleRow": false,
    "columns": [{
        "title": "none",
        "size": 27
    }, {
        "title": "none"
    }],
    "rows": [{
        "instance": "",
        "cells": [
            { "rowTitle": "Device ID" },
            "device_id"
        ]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Average % utilization of all GPU Engines " },
            { "rowTitle": "EU Array Active (%) " },
            { "rowTitle": "EU Array Stall (%) " },
            { "rowTitle": "EU Array Idle (%) " },
            { "rowTitle": " " },
            { "rowTitle": "Compute Engine Util (%) " },
            { "rowTitle": "Render Engine Util (%) " },
            { "rowTitle": "Media Engine Util (%) " },
            { "rowTitle": "Decoder Engine Util (%) " },
            { "rowTitle": "Encoder Engine Util (%) " },
            { "rowTitle": "Copy Engine Util (%) " },
            { "rowTitle": "Media EM Engine Util (%) " },
            { "rowTitle": "3D Engine Util (%) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_UTILIZATION].value", "fixer": "round" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_ACTIVE].value", "scale": 1 }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_STALL].value", "scale": 1 }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_IDLE].value", "scale": 1 }
            ]},
            { "rowTitle": " " },
            { "value": "compute_engine_util"},
            { "value": "render_engine_util"},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION].value", "fixer": "round" }
            ]},
            { "value": "decoder_engine_util"},
            { "value": "encoder_engine_util"},
            { "value": "copy_engine_util"},
            { "value": "media_em_engine_util"},
            { "value": "3d_engine_util"}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Reset" },
            { "rowTitle": "Programming Errors" },
            { "rowTitle": "Driver Errors" },
            { "rowTitle": "Cache Errors Correctable" },
            { "rowTitle": "Cache Errors Uncorrectable" },
            { "rowTitle": "Mem Errors Correctable" },
            { "rowTitle": "Mem Errors Uncorrectable" }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_RESET].value" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS].value" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS].value" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE].value" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE].value" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE].value" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE].value" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU Power (W) " },
            { "rowTitle": "GPU Frequency (MHz) " },
            { "rowTitle": "Media Engine Freq (MHz) " },
            { "rowTitle": "GPU Core Temperature (C) " },
            { "rowTitle": "GPU Memory Temperature (C) " },
            { "rowTitle": "GPU Memory Read (kB/s) " },
            { "rowTitle": "GPU Memory Write (kB/s) " },
            { "rowTitle": "GPU Memory Bandwidth (%) " },
            { "rowTitle": "GPU Memory Used (MiB) " },
            { "rowTitle": "GPU Memory Util (%) " },
            { "rowTitle": "Xe Link Throughput (kB/s) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_POWER].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_FREQUENCY].value" }
            ]}, {"label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEDIA_ENGINE_FREQUENCY].value" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_CORE_TEMPERATURE].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_TEMPERATURE].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_BANDWIDTH].value" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].value", "scale": 1, "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": false, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_UTILIZATION].value", "fixer": "round" }
            ]}, { "value": "fabric_throughput"}
        ]]
    }]
})"_json);
static CharTableConfig ComletConfigDeviceStatisticsDeviceLevel(R"({
    "showTitleRow": false,
    "columns": [{
        "title": "none",
        "size": 27
    }, {
        "title": "none"
    }],
    "rows": [{
        "instance": "",
        "cells": [
            { "rowTitle": "Device ID" },
            "device_id"
        ]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Average % utilization of all GPU Engines " },
            { "rowTitle": "EU Array Active (%) " },
            { "rowTitle": "EU Array Stall (%) " },
            { "rowTitle": "EU Array Idle (%) " },
            { "rowTitle": " " },
            { "rowTitle": "Compute Engine Util (%) " },
            { "rowTitle": "Render Engine Util (%) " },
            { "rowTitle": "Media Engine Util (%) " },
            { "rowTitle": "Decoder Engine Util (%) " },
            { "rowTitle": "Encoder Engine Util (%) " },
            { "rowTitle": "Copy Engine Util (%) " },
            { "rowTitle": "Media EM Engine Util (%) " },
            { "rowTitle": "3D Engine Util (%) " }
        ], [
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_UTILIZATION].value", "fixer": "round" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_ACTIVE].value", "scale": 1 }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_STALL].value", "scale": 1 }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_IDLE].value", "scale": 1 }
            ]},
            { "rowTitle": " " },
            { "value": "compute_engine_util"},
            { "value": "render_engine_util"},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENGINE_GROUP_MEDIA_ALL_UTILIZATION].value", "fixer": "round" }
            ]},
            { "value": "decoder_engine_util"},
            { "value": "encoder_engine_util"},
            { "value": "copy_engine_util"},
            { "value": "media_em_engine_util"},
            { "value": "3d_engine_util"}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "Reset" },
            { "rowTitle": "Programming Errors" },
            { "rowTitle": "Driver Errors" },
            { "rowTitle": "Cache Errors Correctable" },
            { "rowTitle": "Cache Errors Uncorrectable" },
            { "rowTitle": "Mem Errors Correctable" },
            { "rowTitle": "Mem Errors Uncorrectable" }
        ], [
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_RESET].value" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_PROGRAMMING_ERRORS].value" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_DRIVER_ERRORS].value" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_CORRECTABLE].value" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_CACHE_ERRORS_UNCORRECTABLE].value" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_CORRECTABLE].value" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_RAS_ERROR_CAT_NON_COMPUTE_ERRORS_UNCORRECTABLE].value" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU Power (W) " },
            { "rowTitle": "GPU Frequency (MHz) " },
            { "rowTitle": "Media Engine Freq (MHz) " },
            { "rowTitle": "GPU Core Temperature (C) " },
            { "rowTitle": "GPU Memory Temperature (C) " },
            { "rowTitle": "GPU Memory Read (kB/s) " },
            { "rowTitle": "GPU Memory Write (kB/s) " },
            { "rowTitle": "GPU Memory Bandwidth (%) " },
            { "rowTitle": "GPU Memory Used (MiB) " },
            { "rowTitle": "GPU Memory Util (%) " },
            { "rowTitle": "Xe Link Throughput (kB/s) " }
        ], [
            { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_POWER].value", "fixer": "round" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_FREQUENCY].value" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEDIA_ENGINE_FREQUENCY].value" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_CORE_TEMPERATURE].value", "fixer": "round" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_TEMPERATURE].value", "fixer": "round" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].value", "fixer": "round" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].value", "fixer": "round" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_BANDWIDTH].value" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].value", "scale": 1, "fixer": "round" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_UTILIZATION].value", "fixer": "round" }
            ]}, { "value": "fabric_throughput"}
        ]]
    }]
})"_json);
#endif

bool ComletStatistics::hasEUMetrics() {
    return this->opts->showEuMetrics;
}

bool ComletStatistics::hasRASMetrics() {
    return this->opts->showRASMetrics;
}

bool ComletStatistics::hasXelinkMetrics() {
    return this->opts->showXelinkMetrics || this->opts->xelinkThroughputMatrix;
}

void ComletStatistics::setupOptions() {
    this->opts = std::unique_ptr<ComletStatisticsOptions>(new ComletStatisticsOptions());
#ifndef DAEMONLESS
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "The device ID to query");
    auto groupIdOpt = addOption("-g,--group", this->opts->groupId, "The group ID to query");
#else
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "The device ID or PCI BDF address to query");
    addFlag("-e,--eu", this->opts->showEuMetrics, "Show EU metrics");
    addFlag("-r,--ras", this->opts->showRASMetrics, "Show RAS error metrics");
    auto xeLinke = addFlag("-x", this->opts->showXelinkMetrics, "Show Xe Link metrics");
    xeLinke->needs(deviceIdOpt);
#endif

    deviceIdOpt->check([this](const std::string &str) {
    std::string errStr = "Device ID should be a non-negative integer or a BDF string";
    if (isValidDeviceId(str)) {
        return std::string();        
    } else if (isBDF(str)) {
        return std::string();
    }
    return errStr;
    });

    auto xeLinkThroughputMatrixFlag = addFlag("--xelink", this->opts->xelinkThroughputMatrix, "Show the all the Xe Link throughput (GB/s) matrix");

    auto xeLinkUtilMatrixFlag = addFlag("--utils", this->opts->xelinkUtilMatrix, "Show the Xe Link throughput utilization");

    xeLinkThroughputMatrixFlag->excludes(deviceIdOpt);
    xeLinkUtilMatrixFlag->needs(xeLinkThroughputMatrixFlag);
#ifndef DAEMONLESS
    xeLinkThroughputMatrixFlag->excludes(groupIdOpt);
#endif
}

std::unique_ptr<nlohmann::json> ComletStatistics::run() {
    if(this->opts->xelinkThroughputMatrix){
        return this->coreStub->getXelinkThroughputAndUtilMatrix();
    }
    if (isDeviceOp()) {
        int targetId = -1;
        if (isNumber(this->opts->deviceId)) {
            targetId = std::stoi(this->opts->deviceId);
        } else {
            auto convertResult = this->coreStub->getDeivceIdByBDF(this->opts->deviceId.c_str(), &targetId);
            if (convertResult->contains("error")) {
                return convertResult;
            }
        }
        auto json = this->coreStub->getStatistics(targetId, true, true);
        return json;
    } else if (isGroupOp()) {
        auto json = this->coreStub->getStatisticsByGroup(this->opts->groupId, true, true);
        return json;
    }
    auto json = std::unique_ptr<nlohmann::json>(new nlohmann::json());
    (*json)["error"] = "Unknow operation";
    return json;
}

std::string engineUtilFormater(nlohmann::json json, bool intent = false) {
    std::string intent_str = intent ? std::string(2, ' ') : "";
    std::string tmp;
    std::string res;
    int i = 0;
    auto func = [](nlohmann::json obj1, nlohmann::json obj2) {
        return obj1["engine_id"] <= obj2["engine_id"];
    };
    std::sort(json.begin(), json.end(), func);
    for (auto obj : json) {
        if (tmp.empty()) tmp = intent_str;
        #ifndef DAEMONLESS
        tmp += "Engine " + std::to_string(obj["engine_id"].get<int>()) + ": " + std::to_string(obj["avg"].get<int>());
        #else
        tmp += "Engine " + std::to_string(obj["engine_id"].get<int>()) + ": " + std::to_string(obj["value"].get<int>());
        #endif
        i++;
        if (i % 4 == 0) {
            res += tmp + "\n";
            tmp.clear();
        } else {
            tmp += ", ";
        }
    }
    if (!tmp.empty()) {
        res += tmp;
        res.pop_back();
        res.pop_back();
    } else if (i > 0) {
        res.pop_back();
    }
    return res;
}

std::string engineUtilByType(std::shared_ptr<nlohmann::json> jsonPtr, std::string key) {
    std::string res;
    auto findEngineGroupUtilFn = [key](nlohmann::json item) {
        std::string tmpKey(key);
        std::transform(tmpKey.begin(), tmpKey.end(), tmpKey.begin(), toupper);
        tmpKey += "_ALL_UTILIZATION";
        return item["metrics_type"].get<std::string>().find(tmpKey) != std::string::npos;
    };

    // device level
    if (jsonPtr->contains("engine_util") && (*jsonPtr)["engine_util"].contains(key)) {
        auto jsonObj = (*jsonPtr)["engine_util"][key];
        #ifdef DAEMONLESS
        auto found = std::find_if(
            (*jsonPtr)["device_level"].begin(),
            (*jsonPtr)["device_level"].end(),
            findEngineGroupUtilFn
        );
        if (found != (*jsonPtr)["device_level"].end()) {
            res += std::to_string((*found)["value"].get<int>()) + "; ";
        }
        #endif
        res += engineUtilFormater(jsonObj) + "\n";
    }
    // tile level
    if (jsonPtr->contains("tile_level")) {
        for (auto tileJson : (*jsonPtr)["tile_level"]) {
            if (tileJson.contains("engine_util") && tileJson["engine_util"].contains(key)) {
                auto jsonObj = tileJson["engine_util"][key];
                auto engineStr = engineUtilFormater(jsonObj, true);
                #ifdef DAEMONLESS
                auto found = std::find_if(
                    tileJson["data_list"].begin(),
                    tileJson["data_list"].end(),
                    findEngineGroupUtilFn
                );
                #endif
                if (!engineStr.empty()) {
                    res += "Tile " + std::to_string(tileJson["tile_id"].get<int>()) + ":\n";
                    #ifdef DAEMONLESS
                    if (found != tileJson["data_list"].end()) {
                        res +=  "  " + std::to_string((*found)["value"].get<int>()) + "; "
                            + engineUtilFormater(jsonObj, false) + "\n";
                    } else {
                        res += engineUtilFormater(jsonObj, true) + "\n";
                    }
                    #else
                    res += engineUtilFormater(jsonObj, true) + "\n";
                    #endif
                }
            }
        }
    }
    if (!res.empty()) res.pop_back();
    return res;
}

std::string getXelinkThroughput(std::shared_ptr<nlohmann::json> jsonPtr) {
    std::string res;
    if (!jsonPtr->contains("fabric_throughput"))
        return res;
    std::stringstream ss;
    for (auto& obj : (*jsonPtr)["fabric_throughput"]) {
        ss.str("");
        auto key = obj["name"].get<std::string>();
        int i = key.find("->");
        key.insert(i + 2, " ");
        key.insert(i, " ");
        ss << key << ": ";
        #ifndef DAEMONLESS
        ss << "avg: " << obj["avg"] << ", ";
        ss << "min: " << obj["min"] << ", ";
        ss << "max: " << obj["max"] << ", ";
        ss << "current: " << obj["value"];
        #else
        ss << obj["value"];
        #endif
        res += ss.str() + "\n";
    }
    if (!res.empty()) res.pop_back();
    return res;
}

static void showDeviceStatistics(std::ostream& out, std::shared_ptr<nlohmann::json> jsonPtr, const bool cont = false) {
    nlohmann::json& json = *jsonPtr;
    bool noTile = true;
    auto tileJson = json.find("tile_level");
    if (tileJson != json.end()) {
        if (tileJson->is_array() && tileJson->size() >= 2) {
            noTile = false;
        }
    }

    if (noTile) {
        auto deviceJson = json.find("device_level");
        if (deviceJson != json.end() && deviceJson->is_array()) {
            if (tileJson != json.end()) {
                json.erase(tileJson);
            }
            json["tile_level"] = nlohmann::json::array();
            tileJson = json.find("tile_level");
            auto tile0 = nlohmann::json::object();
            tile0["tile_id"] = 0;
            tile0["data_list"] = (*deviceJson);
            tileJson->push_back(tile0);
        }
    }

    json["compute_engine_util"] = engineUtilByType(jsonPtr, "compute");
    json["render_engine_util"] = engineUtilByType(jsonPtr, "render");
    json["decoder_engine_util"] = engineUtilByType(jsonPtr, "decoder");
    json["encoder_engine_util"] = engineUtilByType(jsonPtr, "encoder");
    json["copy_engine_util"] = engineUtilByType(jsonPtr, "copy");
    json["media_em_engine_util"] = engineUtilByType(jsonPtr, "media_enhancement");
    json["3d_engine_util"] = engineUtilByType(jsonPtr, "3d");

    json["fabric_throughput"] = getXelinkThroughput(jsonPtr);

#ifndef DAEMONLESS
    CharTable table(ComletConfigDeviceStatistics, json, cont);
#else
    CharTable table(noTile ? ComletConfigDeviceStatisticsDeviceLevel: ComletConfigDeviceStatistics, json, cont);
#endif

    //Remove 3d Utilization row, if its not supported
    if (!json.contains("3d_engine_util") || json["3d_engine_util"].is_null() || json["3d_engine_util"] == "")
         table.removeRow("3D Engine Util (%) ");

    table.show(out);
}

void ComletStatistics::printHead(std::string head[], int count, int headsize, int rowsize) {
    std::cout << std::left << std::setw(headsize) << "From\\To";
    for (int i = 0; i < count; i++) {
        std::cout << std::left << std::setw(rowsize) << head[i];
    }
    std::cout << std::endl;
}

std::string to_string_with_precision(const double a_value, const int n = 2) {
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return std::move(out).str();
}

void ComletStatistics::printContent(std::string head[], const nlohmann::json &table, int count, int headsize, int rowsize) {
    for (int col = 0; col < count; col++) {
        std::cout << std::left << std::setw(headsize) << head[col];
        for (int row = 0; row < count; row++) {
            std::string value = "---";
            if (this->opts->xelinkThroughputMatrix) {
                if (!this->opts->xelinkUtilMatrix) {
                    if (table[col * count + row].contains("throughput") && table[col * count + row]["throughput"] != -1) {
                        value = to_string_with_precision(table[col * count + row]["throughput"].get<double>());
                    }
                } else {
                    if (table[col * count + row].contains("utilization") && table[col * count + row]["utilization"] != -1) {
                        value = to_string_with_precision(table[col * count + row]["utilization"].get<double>());
                    }
                }
            }
            std::cout << std::left << std::setw(rowsize) << value;
        }
        std::cout << std::endl;
    }
}

void ComletStatistics::printXelinkTable(std::shared_ptr<nlohmann::json> json) {
    if(!json->contains("xelink_stats_list"))
        return;
    const nlohmann::json &table = (*json)["xelink_stats_list"];
    int nCount = table.size();
    int instance = sqrt(nCount);
    std::string title[instance];
    int headsize = 9;
    int rowsize = 9;

    for (int i = 0; i < instance; i++) {
        title[i] = "GPU " + getKeyNumberValue("remote_device_id", table[i]) + "/" + getKeyNumberValue("remote_subdevice_id", table[i]);
    }

    printHead(title, instance, headsize, rowsize);
    printContent(title, table, instance, headsize, rowsize);
}

void ComletStatistics::getTableResult(std::ostream& out) {
    auto res = run();
    if (res->contains("error")) {
        out << "Error: " << (*res)["error"].get<std::string>() << std::endl;
        setExitCodeByJson(*res);
        return;
    }
    std::shared_ptr<nlohmann::json> json = std::make_shared<nlohmann::json>();
    *json = *res;
    if (this->opts->xelinkThroughputMatrix) {
        printXelinkTable(json);
    } else if (this->opts->groupId != 0) {
        auto devices = (*json)["datas"].get<std::vector<nlohmann::json>>();
        bool cont = false;
        for (auto device : devices) {
            showDeviceStatistics(out, std::make_shared<nlohmann::json>(device), cont);
            cont = true;
        }
    } else {
        showDeviceStatistics(out, json);
    }
}

} // end namespace xpum::cli
