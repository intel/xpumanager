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
 * When upgrade pcm, make sure the directory 'include' is placed in the root directory.
 * And pcm-iio-gpu.cpp and CMakeLists.txt are placed in the same directory as pcm-iio.cpp 
 * while pcm-iio-gpu.h and pcm-iio-gpu.cpp are on the right path.
 */

#include <vector>
#include <string>

int pcm_iio_gpu_init();

std::vector<std::string> pcm_iio_gpu_query();

#endif