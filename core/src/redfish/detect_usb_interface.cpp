/*
 *  Copyright (C) 2021-2022 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file detect_usb_interface.cpp
 */

#define _XOPEN_SOURCE 500
#include "detect_usb_interface.h"
#include <ftw.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <iostream>
#include <fstream>

using namespace std;

static string basePath = "/sys/bus/usb/devices";

static int level_max = 3;
static int idVendor;
static int idProduct;


static std::string devPath;
static std::string interfaceName;

static int
find_dev_path(const char *fpath,
              const struct stat *sb,
              int tflag,
              struct FTW *ftwbuf)
{
    int ret = ftwbuf->level > level_max ? FTW_SKIP_SUBTREE : FTW_CONTINUE;
    if (tflag == FTW_D)
    {
        std::string path(fpath);
        ifstream f_id_vendor(path + "/idVendor");
        string line;
        if (!f_id_vendor.is_open())
        {
            return ret;
        }
        else
        {
            getline(f_id_vendor, line);
            int id_vendor = std::stoi(line, 0, 16);
            if (id_vendor != idVendor)
            {
                return ret;
            }
            f_id_vendor.close();
        }
        ifstream f_id_product(path + "/idProduct");
        line.clear();
        if (!f_id_product.is_open())
        {
            return ret;
        }
        else
        {
            getline(f_id_product, line);
            int id_product = std::stoi(line, 0, 16);
            if (id_product != idProduct)
            {
                return ret;
            }
            f_id_product.close();
        }
        devPath = path;
        return FTW_STOP;
    }
    return ret;
}

static int find_interface_name(const char *fpath,
                               const struct stat *sb,
                               int tflag,
                               struct FTW *ftwbuf)
{
    int ret = ftwbuf->level > level_max ? FTW_SKIP_SUBTREE : FTW_CONTINUE;
    if (tflag == FTW_D)
    {
        std::string path(fpath);
        std::string filename = path.substr(ftwbuf->base);
        if (filename.compare("net") != 0)
            return ret;
        DIR *dir;
        struct dirent *ent;
        if ((dir = opendir(path.c_str())) != NULL)
        {
            /* print all the files and directories within directory */
            while ((ent = readdir(dir)) != NULL)
            {
                if (ent->d_name[0] == '.')
                    continue;
                interfaceName = ent->d_name;
                closedir(dir);
                return FTW_STOP;
            }
            closedir(dir);
        }
    }
    return ret;
}

std::string getUsbInterfaceName(std::string idVendorStr, std::string idProductStr) {
    idVendor = std::stoi(idVendorStr, 0, 16);
    idProduct = std::stoi(idProductStr, 0, 16);
    int err = nftw(basePath.c_str(), find_dev_path, 1, FTW_ACTIONRETVAL);
    if (err == -1) {
        return "";
    }
    err = nftw(devPath.c_str(), find_interface_name, 1, FTW_ACTIONRETVAL);
    if (err == -1) {
        return "";
    }
    return interfaceName;
}