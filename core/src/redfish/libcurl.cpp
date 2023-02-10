/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file libcurl.cpp
 */

#include "libcurl.h"

std::string getLibCurlPath() {
    std::string cmd = "ldconfig -p 2>&1";
    FILE *f = popen(cmd.c_str(), "r");
    char c_line[1024];

    std::vector<CurlLibVersion> libList;

    while (fgets(c_line, 1024, f) != NULL) {
        std::string line(c_line);
        auto idx = line.find("libcurl.so");
        if (idx != line.npos) {
            line = line.substr(idx);
            auto endIdx = line.find(' ');
            if (endIdx != line.npos) {
                std::string name = line.substr(0, endIdx);
                if (name.length() > 0) {
                    CurlLibVersion lib(name);
                    if (lib.valid)
                        libList.push_back(lib);
                }
            }
        }
    }
    pclose(f);
    if (libList.size() > 0) {
        auto &lib = libList.at(0);
        for (size_t i = 1; i < libList.size(); i++) {
            if (lib < libList.at(i)) {
                lib = libList.at(i);
            }
        }
        return lib.name;
    }
    return "libcurl.so";
}