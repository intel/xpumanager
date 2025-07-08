/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file win_native.h
 */

#pragma once

void openPDHQuery();

void updatePDHQuery();

void closePDHQuery();

double getComputeEngineUtilByNativeAPI();

double getCopyEngineUtilByNativeAPI();

double getMediaEngineUtilByNativeAPI();

double getRenderEngineUtilByNativeAPI();

double getMemSizeByNativeAPI();

double getMemUsedByNativeAPI();