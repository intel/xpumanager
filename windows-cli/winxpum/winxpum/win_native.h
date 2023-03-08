/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file win_native.h
 */

#pragma once

void initPDHQuery();

void updatePDHQuery();

void closePDHQuery();

double getComputeEngineUtilByNativeAPI();

double getCopyEngineUtilByNativeAPI();

double getMediaEngineUtilByNativeAPI();

double getMemSizeByNativeAPI();

double getMemUsedByNativeAPI();