/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file igsc_manager.cpp
 */
#include <string>
#include <unordered_map>

class IGSC_Manager {

   public:
    IGSC_Manager() {

    }

    int init();

    std::string getDeviceGSCVersion(std::string bdf);

    std::string getDeviceGSCDataVersion(std::string bdf);

    int runFlashGSC(std::string bdf, std::string image_file, bool force);

    int runFlashGSCData(std::string bdf, std::string image_file);

    bool setDeviceEccState(std::string bdf, uint8_t req_state, uint8_t* cur_state, uint8_t* pen_state);

    bool getDeviceEccState(std::string bdf, uint8_t* cur_state, uint8_t* pen_state);

    bool isFwImageAndDeviceCompatible(std::string bdf, std::string image_file);

    bool isFwDataImageAndDeviceCompatible(std::string bdf, std::string image_file, std::string& error_message);

   private:
    bool initialized = false;

    std::unordered_map<std::string, std::string> bdf_to_devicepath;
};