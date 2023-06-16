/* 
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file pcm-iio-gpu.h
 */

#ifndef _PCM_IIO_GPU_API_H
#define _PCM_IIO_GPU_API_H

/**
 * A simple library for Intel GPU PCIe throughput collection 
 *
 * Important files:
 *  - inlcude/pcm-iio-gpu.h
 *  - pcm-iio-gpu.cpp
 *  - CMakeLists.txt
 * 
 */

#include <vector>
#include <string>

int pcm_iio_gpu_init();

std::vector<std::string> pcm_iio_gpu_query();

#endif