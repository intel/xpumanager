/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file comlet_statistics.cpp
 */

#include "comlet_statistics.h"

#include <map>
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
            { "rowTitle": "GPU Utilization (%) " },
            { "rowTitle": "EU Array Active (%) " },
            { "rowTitle": "EU Array Stall (%) " },
            { "rowTitle": "EU Array Idle (%) " }
        ], [
            { "value": "begin" },
            { "value": "end" },
            { "value": "elapsed_time" },
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_ENERGY].value", "scale": 1000 }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_UTILIZATION].avg", "fixer": "round" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_ACTIVE].avg" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_STALL].avg" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_IDLE].avg" }
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
                { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].avg" },
                { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].min" },
                { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].max" },
                { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].value" }
            ]}
        ]]
    }, {
        "instance": "",
        "cells": [[
            { "rowTitle": "GPU Memory Write (kB/s) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].avg" },
                { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].min" },
                { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].max" },
                { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].value" }
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
                { "label": "avg", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].avg", "scale": 1048576, "fixer": "round" },
                { "label": "min", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].min", "scale": 1048576, "fixer": "round" },
                { "label": "max", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].max", "scale": 1048576, "fixer": "round" },
                { "label": "current", "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].value", "scale": 1048576, "fixer": "round" }
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
            { "rowTitle": "GPU Utilization (%) " },
            { "rowTitle": "EU Array Active (%) " },
            { "rowTitle": "EU Array Stall (%) " },
            { "rowTitle": "EU Array Idle (%) " },
            { "rowTitle": " " },
            { "rowTitle": "Compute Engine Util (%) " },
            { "rowTitle": "Render Engine Util (%) " },
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
                { "value": "data_list[metrics_type==XPUM_STATS_EU_ACTIVE].value" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_STALL].value" }
            ]},
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_IDLE].value" }
            ]},
            { "value": " "},
            { "value": "compute_engine_util"},
            { "value": "render_engine_util"},
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
            { "rowTitle": "GPU Core Temperature (C) " },
            { "rowTitle": "GPU Memory Temperature (C) " },
            { "rowTitle": "GPU Memory Read (kB/s) " },
            { "rowTitle": "GPU Memory Write (kB/s) " },
            { "rowTitle": "GPU Memory Bandwidth (%) " },
            { "rowTitle": "GPU Memory Used (MiB) " },
            { "rowTitle": "Xe Link Throughput (kB/s) " }
        ], [
            { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_POWER].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_FREQUENCY].value" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_CORE_TEMPERATURE].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_TEMPERATURE].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_READ_THROUGHPUT].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_WRITE_THROUGHPUT].value", "fixer": "round" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_BANDWIDTH].value" }
            ]}, { "label": "Tile ", "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].value", "scale": 1048576, "fixer": "round" }
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
            { "rowTitle": "GPU Utilization (%) " },
            { "rowTitle": "EU Array Active (%) " },
            { "rowTitle": "EU Array Stall (%) " },
            { "rowTitle": "EU Array Idle (%) " },
            { "rowTitle": " " },
            { "rowTitle": "Compute Engine Util (%) " },
            { "rowTitle": "Render Engine Util (%) " },
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
                { "value": "data_list[metrics_type==XPUM_STATS_EU_ACTIVE].value" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_STALL].value" }
            ]},
            { "label_tag": "tile_id", "value": "tile_level[]", "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_EU_IDLE].value" }
            ]},
            { "value": " "},
            { "value": "compute_engine_util"},
            { "value": "render_engine_util"},
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
            { "rowTitle": "GPU Core Temperature (C) " },
            { "rowTitle": "GPU Memory Temperature (C) " },
            { "rowTitle": "GPU Memory Read (kB/s) " },
            { "rowTitle": "GPU Memory Write (kB/s) " },
            { "rowTitle": "GPU Memory Bandwidth (%) " },
            { "rowTitle": "GPU Memory Used (MiB) " },
            { "rowTitle": "Xe Link Throughput (kB/s) " }
        ], [
            { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_POWER].value", "fixer": "round" }
            ]}, { "label_tag": "tile_id", "value": "tile_level[]", "subrow": true, "subs": [
                { "value": "data_list[metrics_type==XPUM_STATS_GPU_FREQUENCY].value" }
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
                { "value": "data_list[metrics_type==XPUM_STATS_MEMORY_USED].value", "scale": 1048576, "fixer": "round" }
            ]}, { "value": "fabric_throughput"}
        ]]
    }]
})"_json);
#endif

bool ComletStatistics::hasEUMetrics() {
    return this->opts->showEuMetrics;
}

void ComletStatistics::setupOptions() {
    this->opts = std::unique_ptr<ComletStatisticsOptions>(new ComletStatisticsOptions());
#ifndef DAEMONLESS
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "The device ID to query");
    addOption("-g,--group", this->opts->groupId, "The group ID to query");
#else
    auto deviceIdOpt = addOption("-d,--device", this->opts->deviceId, "The device ID or PCI BDF address to query");
    addFlag("-e,--eu", this->opts->showEuMetrics, "Show the EU metrics");
#endif

    deviceIdOpt->check([this](const std::string &str) {
#ifndef DAEMONLESS
    std::string errStr = "Device id should be integer larger than or equal to 0";
    if (!isValidDeviceId(str)) {
        return errStr;
    }
    return std::string();
#else
    std::string errStr = "Device id should be a non-negative integer or a BDF string";
    if (isValidDeviceId(str)) {
        return std::string();        
    } else if (isBDF(str)) {
        return std::string();
    }
    return errStr;
#endif
    });
}

std::unique_ptr<nlohmann::json> ComletStatistics::run() {
    if (isDeviceOp()) {
        if (isNumber(this->opts->deviceId)) {
            auto json = this->coreStub->getStatistics(std::stoi(this->opts->deviceId), true);
            return json;
        } else {
            auto json = this->coreStub->getStatistics(this->opts->deviceId.c_str(), true);
            return json;
        }
    } else if (isGroupOp()) {
        auto json = this->coreStub->getStatisticsByGroup(this->opts->groupId, true);
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

    // device level
    if (jsonPtr->contains("engine_util") && (*jsonPtr)["engine_util"].contains(key)) {
        auto jsonObj = (*jsonPtr)["engine_util"][key];
        res += engineUtilFormater(jsonObj) + "\n";
    }
    // tile level
    if (jsonPtr->contains("tile_level")) {
        for (auto tileJson : (*jsonPtr)["tile_level"]) {
            if (tileJson.contains("engine_util") && tileJson["engine_util"].contains(key)) {
                auto jsonObj = tileJson["engine_util"][key];
                auto engineStr = engineUtilFormater(jsonObj, true);
                if (!engineStr.empty()) {
                    res += "Tile " + std::to_string(tileJson["tile_id"].get<int>()) + ":\n";
                    res += engineUtilFormater(jsonObj, true) + "\n";
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
    table.show(out);
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
    if (this->opts->groupId != 0) {
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
