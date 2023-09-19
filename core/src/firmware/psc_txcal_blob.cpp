/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file psc_txcal_blob.cpp
 */

#include <dirent.h>
#include <sys/stat.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <streambuf>
#include <string>

#include "api/psc.h"
#include "psc_txcal_blob.h"
#include "infrastructure/logger.h"

namespace xpum {

static const std::string DEV_TOP = "/sys/devices";

static std::string getParentDirectory(const std::string& path) {
    size_t last_slash_idx = path.rfind('/');
    if (last_slash_idx == std::string::npos) {
        return "";
    }
    return path.substr(0, last_slash_idx);
}

static std::string getFilename(const std::string& path) {
    size_t last_slash_idx = path.find_last_of("/");
    if (last_slash_idx == std::string::npos) {
        return path;
    }
    return path.substr(last_slash_idx + 1);
}

static bool startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() &&
           str.compare(0, prefix.size(), prefix) == 0;
}

/**
 * recursively find full file name according to pattern under a dir
*/
static void recursiveFindFilenameUnderDirectory(const std::string& directory, const std::string& pattern, std::vector<std::string>& results) {
    DIR* dir;
    struct dirent* entry;

    if (!(dir = opendir(directory.c_str())))
        return;

    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            std::string dname = std::string(entry->d_name);
            if (dname == "." || dname == "..")
                continue;
            recursiveFindFilenameUnderDirectory(directory + "/" + dname, pattern, results);
        }
        std::string fname = std::string(entry->d_name);
        if (fname == pattern) {
            results.push_back(directory + "/" + fname);
        }
    }
    closedir(dir);
}

static std::string getMeiDeviceNode(const std::string& mei_device) {
    std::string node;
    std::vector<std::string> pathList;
    recursiveFindFilenameUnderDirectory(DEV_TOP, mei_device, pathList);
    if (pathList.size() <= 0) {
        XPUM_LOG_TRACE("Coundn't find mei device node for {}", mei_device);
        return node;
    }
    XPUM_LOG_TRACE("{} files match {}", pathList.size(), mei_device);
    node = pathList.at(0);
    std::string name = getFilename(node);
    while (startsWith(name, "mei") || startsWith(name, "i915.mei")) {
        node = getParentDirectory(node);
        name = getFilename(node);
    }
    XPUM_LOG_TRACE("mei device node for {} is {}", mei_device, node);
    return node;
}

static std::string findFilenameUnderDirectory(const std::string& directory, const std::string& pattern) {
    std::regex e(pattern);
    DIR* dir;
    struct dirent* entry;
    std::string path;

    if (!(dir = opendir(directory.c_str())))
        return "";

    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            if (std::string(entry->d_name) == "." || std::string(entry->d_name) == "..")
                continue;
            if (std::regex_search(std::string(entry->d_name), e)) {
                path = directory + "/" + std::string(entry->d_name);
                break;
            }
        }
    }
    closedir(dir);
    return path;
}

static std::string readTextFile(const std::string& path) {
    std::string content;
    std::ifstream t(path);
    if (t) {
        t.seekg(0, t.end);
        auto size = t.tellg();
        t.seekg(0, t.beg);
        if (size <= 8192) {
            t >> content;
        }
        t.close();
    }
    return content;
}

static std::string getPscNvmemPath(const std::string& node) {
    std::string path = findFilenameUnderDirectory(node, "i915[\\._\\-]spi\\..*");
    if (!path.length()) {
        XPUM_LOG_TRACE("Coundn't find i915[._-]spi in folder {}", node);
        return path;
    }
    XPUM_LOG_TRACE("Find SPI folder {}", path);
    path = findFilenameUnderDirectory(path, "mtd");
    XPUM_LOG_TRACE("Find mtd folder {}", path);
    std::vector<std::string> names;
    recursiveFindFilenameUnderDirectory(path, "name", names);

    XPUM_LOG_TRACE("Find {} 'name' files", names.size());

    std::string psc;
    for (const auto& name : names) {
        std::string content = readTextFile(name);
        XPUM_LOG_TRACE("{}:\n{}", name, content);
        std::regex r(".*\\.PSC");
        if (std::regex_match(content, r)) {
            XPUM_LOG_TRACE("Match 'PSC' in {}", name);
            psc = getParentDirectory(name);
            break;
        }
    }

    return psc + "/" + getFilename(psc) + "/nvmem";
}

static uint32_t crc32c(const std::vector<uint8_t>& byte_data, uint32_t crc = 0) {
    uint32_t crc32c_polynomial = 0x82f63b78;
    for (uint8_t each_byte : byte_data) {
        crc ^= each_byte;
        for (int i = 0; i < 8; ++i) {
            crc = (crc >> 1) ^ ((crc & 1) ? crc32c_polynomial : 0);
        }
    }
    return crc;
}

#define NVMEM_HEADER_SIZE 8192
#define NVMEM_BLOB_SIZE_LIMIT ((1024 * 1024) * 2 + NVMEM_HEADER_SIZE)
static std::vector<uint8_t> getNvmemData(const std::string& nvmem) {
    if (nvmem.length() == 0) {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> buf(NVMEM_BLOB_SIZE_LIMIT);
    FILE* fp = fopen(nvmem.c_str(), "rb");
    if (!fp) {
        return std::vector<uint8_t>();
    }
    size_t ret = fread(reinterpret_cast<char*>(buf.data()), sizeof(char), NVMEM_BLOB_SIZE_LIMIT, fp);

    if (ret <= NVMEM_HEADER_SIZE) {
        XPUM_LOG_TRACE("{} size <= 8192", nvmem);
        fclose(fp);
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> blob_data(buf.begin() + NVMEM_HEADER_SIZE, buf.end());
    fclose(fp);

    return blob_data;
}

std::vector<uint8_t> getPSCData(std::vector<uint8_t>& blob_data) {
    if (blob_data.size() < offsetof(psc_data, data)) {
        return std::vector<uint8_t>();
    }

    psc_data* psc_hdr = (psc_data*)blob_data.data();

    if (psc_hdr->identifier.magic != PSCBIN_MAGIC_NUMBER) {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> psc_hdr_data(blob_data.begin(), blob_data.begin() + offsetof(psc_data, crc32c_hdr));

    if (psc_hdr->crc32c_hdr != crc32c(psc_hdr_data)) {
        return std::vector<uint8_t>();
    }

    uint32_t format_version = psc_hdr->identifier.psc_format_version;

    if (format_version != PSCBIN_VERSION_NULL && (format_version < PSCBIN_VERSION_MIN || format_version > PSCBIN_VERSION_MAX)) {
        return std::vector<uint8_t>();
    }

    uint32_t psc_size = psc_hdr->data_size + offsetof(psc_data, data);

    if (blob_data.size() < psc_size) {
        return std::vector<uint8_t>();
    }

    return std::vector<uint8_t>(blob_data.begin(), blob_data.begin() + psc_size);
}

static std::vector<uint8_t> getTxCalBlobData(std::vector<uint8_t>& blob_data) {
    uint32_t hdrLen = offsetof(txcal_blob, data);

    if (blob_data.size() < hdrLen) {
        return std::vector<uint8_t>();
    }

    txcal_blob* txcal_hdr = (txcal_blob*)blob_data.data();

    if (txcal_hdr->magic[0] != TXCAL_BLOB_MAGIC_0 ||
        txcal_hdr->magic[1] != TXCAL_BLOB_MAGIC_1 ||
        txcal_hdr->magic[2] != TXCAL_BLOB_MAGIC_2 ||
        txcal_hdr->magic[3] != TXCAL_BLOB_MAGIC_3) {
        return std::vector<uint8_t>();
    }

    uint32_t dataLen = txcal_hdr->num_settings * sizeof(txcal_settings);

    if (blob_data.size() < dataLen + hdrLen) {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> txcal_hdr_data(blob_data.begin(), blob_data.begin() + offsetof(txcal_blob, crc32c_hdr));

    auto computed_hdr_crc = crc32c(txcal_hdr_data);
    if (txcal_hdr->crc32c_hdr != computed_hdr_crc) {
        return std::vector<uint8_t>();
    }
    auto txcal_data_begin = blob_data.begin() + hdrLen;
    std::vector<uint8_t> txcal_data(txcal_data_begin, txcal_data_begin + dataLen);

    auto computed_data_crc = crc32c(txcal_data);

    if (txcal_hdr->crc32c_data != computed_data_crc) {
        return std::vector<uint8_t>();
    }

    if (txcal_hdr->format_version != TXCAL_VERSION_CURRENT) {
        return std::vector<uint8_t>();
    }

    if (txcal_hdr->size != hdrLen + dataLen) {
        return std::vector<uint8_t>();
    }

    return std::vector<uint8_t>(blob_data.begin(), blob_data.begin() + txcal_hdr->size);
}

std::vector<uint8_t> getTxCalBlobByMeiDevice(const std::string& meiDeviceName) {
    auto meiDeviceNodePath = getMeiDeviceNode(meiDeviceName);
    if (meiDeviceNodePath.length() == 0) {
        return std::vector<uint8_t>();
    }
    auto nvmemPath = getPscNvmemPath(meiDeviceNodePath);
    if (nvmemPath.length() == 0) {
        return std::vector<uint8_t>();
    }
    auto blob_data = getNvmemData(nvmemPath);
    if (blob_data.size() == 0) {
        return std::vector<uint8_t>();
    }
    auto psc_data = getPSCData(blob_data);
    if (psc_data.size() == 0) {
        return std::vector<uint8_t>();
    }

    std::vector<uint8_t> txcal_blob_data(blob_data.begin() + psc_data.size(), blob_data.end());
    return getTxCalBlobData(txcal_blob_data);
}

std::string getTxCalDateByMeiDevice(const std::string& meiDeviceName) {
    std::string default_res = "Not Calibrated";
    auto blob_data = getTxCalBlobByMeiDevice(meiDeviceName);
    if (blob_data.size() == 0) {
        return default_res;
    }
    txcal_blob* hdr = (txcal_blob*)blob_data.data();
    char buffer[100];
    int cx;

    cx = snprintf(buffer, 100, "%04x", hdr->date);
    if (cx >= 0 && cx < 100) {
        return std::string(buffer);
    }
    return default_res;
}

std::string getMeiDeviceNameFromPath(const std::string& meiDevicePath) {
    return getFilename(meiDevicePath);
}

} // namespace xpum