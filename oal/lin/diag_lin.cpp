/*
 *
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <array>
#include <filesystem>
#include <dirent.h>
#include <vector>
#include <debug.h>
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <algorithm>
#include "lin.h"

#define MEDIA_CODER_TOOLS "/usr/bin/sample_multi_transcode"
#define MEDIA_DATA_PATH "resources/mediadata/"
#define MEDIA_CODER_TOOLS_1080P_FILE "test_stream_1080p.265"
#define MEDIA_CODER_TOOLS_4K_FILE "test_stream_4K.265"
#define MEDIA_CODEC_TOOLS_LIGHT_FILE "test_stream.264"

/**
 * @brief Checks if the entry name is a renderD related device entry
 *
 * This function determines whether the provided entry name is related
 * to a renderD device entry
 *
 * @param[in] entryName Pointer to entry string
 * @return true if entryName is related to renderD, false otherwise
 */
bool countDevEntry(const std::string &entryName)
{
	constexpr std::string_view prefix = "renderD";

	if (!entryName.starts_with(prefix)) {
		return false;
	}

	return std::all_of(entryName.begin() + prefix.size(), entryName.end(), [](char ch) { return std::isdigit(ch); });
}

/**
 * @brief Check host memory size
 *
 * This function determines if can read the sysinfo and get the host memory size
 *
 * @param[in, out] hostMemorySize Pointer to an uint64_t variable
 * @return true if can read sysinfo successfully, false otherwise
 */
bool checkHostMemorySize(uint64_t *hostMemorySize)
{
	struct sysinfo info;
	if (sysinfo(&info) == 0) {
		*hostMemorySize = (uint64_t)info.totalram * info.mem_unit;
		return true;
	} else {
		return false;
	}
}
/**
 * @brief check Permission
 *
 * This function check access permission for renderD related folders under /dev/dri.
 * @param null
 * @return true if has the access , false otherwise
 */
bool checkPermission()
{
	constexpr std::string_view dirName = "/dev/dri";

	std::error_code ec;
	if (!std::filesystem::exists(dirName, ec) || ec) {
		return false;
	}

	try {
		for (const auto &entry : std::filesystem::directory_iterator(dirName, ec)) {
			if (ec) {
				return false;
			}

			const std::string entryName = entry.path().filename().string();
			if (countDevEntry(entryName)) {
				if (access(entry.path().c_str(), R_OK) != 0) {
					return false;
				}
			}
		}
		return true;
	} catch (const std::filesystem::filesystem_error &) {
		return false;
	}
}

/**
 * @brief Checks if the device entry name exists in the system uevents
 *
 * This function determines whether the provided device entry name exists in the system uevents
 * and returns the related bdf
 *
 * @param[in] bdf Pointer to device bdf entry string from the system uevent folders
 * @param[in] dName Pointer to entry string
 * @return true if the device entry name exists in the system uevent folders, false otherwise
 */
bool checkUevent(std::string &bdf, const char *dName)
{
	bool ret = false;
	char buf[1024];
	char path[PATH_MAX];
	if (dName == NULL) {
		return false;
	}
	snprintf(path, PATH_MAX, "/sys/class/drm/%s/device/uevent", dName);
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		return false;
	}
	int cnt = static_cast<int>(read(fd, buf, 1024));
	if (cnt < 0 || cnt >= 1024) {
		close(fd);
		return false;
	}
	buf[cnt] = 0;
	std::string str(buf);
	std::string key = "PCI_ID=8086:";
	auto pos = str.find(key);
	if (pos == std::string::npos) {
		close(fd);
		return ret;
	}
	key = "PCI_SLOT_NAME=";
	pos = str.find(key);
	if (pos != std::string::npos) {
		bdf = str.substr(pos + key.length(), 12);
	} else {
		close(fd);
		return ret;
	}
	ret = true;
	close(fd);
	return ret;
}

/**
 * @brief Checks if the device bdf exists in the sys fs and system uevent folder
 *
 * This function determines whether the provided device bdf exists in the sys fs and system uevents
 * and return the related device entry string under /dev/dri
 *
 * @param[in] pciBDFStr the device bdf string
 * @return the related device entry string under /dev/dri
 */
std::string getDevicePath(std::string pciBDFStr)
{
	DIR *pdir = NULL;
	struct dirent *pdirent = NULL;
	int len = 0;
	std::string ret = "";
	std::string uBDF;

	pdir = opendir("/sys/class/drm");
	if (pdir == NULL) {
		return ret;
	}

	while ((pdirent = readdir(pdir)) != NULL) {
		if (pdirent->d_name[0] == '.') {
			continue;
		}
		if (strncmp(pdirent->d_name, "render", 6) != 0) {
			continue;
		}
		if (strstr(pdirent->d_name, "-") != NULL) {
			continue;
		}
		if (checkUevent(uBDF, pdirent->d_name) == false) {
			continue;
		}
		len = static_cast<int>(pciBDFStr.size());
		if (len > 0 && strstr(uBDF.c_str(), pciBDFStr.c_str()) != NULL) {
			ret = "/dev/dri/";
			ret += pdirent->d_name;
			break;
		}
	}
	closedir(pdir);
	return ret;
}

/* @brief check Media Codec
 *
 * This function check media codec for a specific device based on device bdf and check needs.
 * @param[in] bdfStr indicates a specific device bdf address string
 * @param[in] functionalCheck indicates the check needs, true for only light requirement, false for full check
 * @param[out] finalResult reference to the final result string
 * @return true if the process successfully, false otherwise
 */
bool checkMediaCodec(std::string &bdfStr, bool functionalCheck, std::string &finalResult)
{
	std::error_code ec;
	std::string testCommand;
	std::string desc;

	std::string devicePath = getDevicePath(bdfStr);
	if (devicePath.empty()) {
		finalResult = "No device path.";
		return false;
	}

	// Find media data directory
	std::string mediaDataPath = findResourceFile(MEDIA_DATA_PATH);
	if (!std::filesystem::exists(mediaDataPath, ec) || ec) {
		finalResult = "No media data path.";
		return false;
	}

	std::ifstream fileTranscode(MEDIA_CODER_TOOLS);
	bool sampleMultiTranscodeToolExist = (fileTranscode.good());

	std::ifstream fileH2651080p(mediaDataPath + MEDIA_CODER_TOOLS_1080P_FILE);
	bool h2651080pFileExist = (fileH2651080p.good());

	std::ifstream fileH2654k(mediaDataPath + MEDIA_CODER_TOOLS_4K_FILE);
	bool h2654kFileExist = (fileH2654k.good());

	std::ifstream fileH264Light(mediaDataPath + MEDIA_CODEC_TOOLS_LIGHT_FILE);
	bool h264LightFileExist = (fileH264Light.good());

	if (!sampleMultiTranscodeToolExist) {
		finalResult = "No sample multi transcode tool.";
		return false;
	}

	if ((!functionalCheck && !h2651080pFileExist && !h2654kFileExist) || (functionalCheck && !h264LightFileExist)) {
		finalResult = "No Media test file.";
		return false;
	}

	if (functionalCheck && h264LightFileExist) {
		testCommand = std::string(MEDIA_CODER_TOOLS) + " -device " + devicePath + " -hw -i::h264 " + mediaDataPath +
					  MEDIA_CODEC_TOOLS_LIGHT_FILE + " -o::h264 null -n 2 2>&1";
		DBG("Transcoding capability check command: %s\n", testCommand.c_str());
		SystemCommandResult cResult = execCommand(testCommand.c_str());
		if (cResult.exitStatus()) {
			finalResult = std::string(MEDIA_CODEC_TOOLS_LIGHT_FILE) + ":" + cResult.output();
			return false;
		}
	}

	if (!functionalCheck && h2651080pFileExist) {

		testCommand = std::string(MEDIA_CODER_TOOLS) + " -device " + devicePath + " -hw -i::h265 " + mediaDataPath +
					  MEDIA_CODER_TOOLS_1080P_FILE + " -o::h265 /tmp/" + MEDIA_CODER_TOOLS_1080P_FILE + " 2>&1";
		DBG("Transcoding capability check command: %s\n", testCommand.c_str());
		SystemCommandResult cResult = execCommand(testCommand.c_str());
		if (cResult.exitStatus()) {
			finalResult = std::string(MEDIA_CODER_TOOLS_1080P_FILE) + ":" + cResult.output();
			return false;
		}
	}

	if (!functionalCheck && h2654kFileExist) {
		testCommand = std::string(MEDIA_CODER_TOOLS) + " -device " + devicePath + " -hw -i::h265 " + mediaDataPath +
					  MEDIA_CODER_TOOLS_4K_FILE + " -o::h265 /tmp/MEDIA_CODER_TOOLS_4K_FILE 2>&1";

		DBG("Transcoding capability check command: %s\n", testCommand.c_str());
		SystemCommandResult cResult = execCommand(testCommand.c_str());
		if (cResult.exitStatus()) {
			finalResult = std::string(MEDIA_CODER_TOOLS_4K_FILE) + ":" + cResult.output();
			return false;
		}
	}

	finalResult = "Success to pass the media transcode checking in ";
	finalResult += (functionalCheck) ? "functionalCheck" : "non-functionalCheck";
	return true;
}

/* @brief check if path exist
 *
 * This function check if path exist.
 * @param[in] s indicates a file path string
 * @return true if the path exist, false otherwise
 */
bool isPathExist(const std::string &s)
{
	struct stat buffer;
	return (stat(s.c_str(), &buffer) == 0);
}
/* @brief get one line of a file content
 *
 * This function get one line of a file content.
 * @param[in] filePath indicates a file path string
 * @param[in] fileName indicates a file name string
 * @return one line of a file content if the process successfully, null otherwise
 */
std::string getOneLineFileContent(std::string filePath, std::string fileName)
{
	std::string line;
	std::ifstream file(filePath + "/" + fileName);
	if (file.is_open()) {
		getline(file, line);
		file.close();
	}
	return line;
}

/* @brief get Downgraded PCIe info
 *
 * This function get Downgraded PCIe info for a specific device based on device bdf.
 * @param[in] bdfStr indicates a specific device bdf string
 * @return std::string if the process successfully, null otherwise
 */
std::string getDowngradedPCIeInfo(std::string &bdfStr)
{
	std::string ret;
	char linkPath[PATH_MAX];
	DIR *pdir = NULL;
	struct dirent *pdirent = NULL;
	int len = 0;
	std::string cardFullPath = "";

	pdir = opendir("/sys/class/drm");
	if (pdir == NULL) {
		return ret;
	}

	while ((pdirent = readdir(pdir)) != NULL) {
		if (pdirent->d_name[0] == '.') {
			continue;
		}
		if (strncmp(pdirent->d_name, "card", 4) != 0) {
			continue;
		}
		if (strstr(pdirent->d_name, "-") != NULL) {
			continue;
		}
		len = snprintf(linkPath, PATH_MAX, "/sys/class/drm/%s", pdirent->d_name);
		if (len <= 0 || len >= PATH_MAX) {
			break;
		}
		char fullPath[PATH_MAX];
		ssize_t fullLen = ::readlink(linkPath, fullPath, sizeof(fullPath));
		if (fullLen < 0) {
			fullLen = 0;
		}
		if (fullLen >= PATH_MAX) {
			fullLen = PATH_MAX - 1;
		}
		fullPath[fullLen] = '\0';

		if (strstr(fullPath, bdfStr.c_str()) != NULL) {
			cardFullPath = "/sys/class/drm/" + std::string(fullPath);
			break;
		}
	}
	closedir(pdir);
	std::string cs = "current_link_speed", cw = "current_link_width", ms = "max_link_speed", mw = "max_link_width";
	while (cardFullPath != "/sys/class/drm") {
		if (isPathExist(cardFullPath + "/" + cs) && isPathExist(cardFullPath + "/" + cw) &&
			isPathExist(cardFullPath + "/" + ms) && isPathExist(cardFullPath + "/" + mw)) {
			std::string currentBridge = cardFullPath.substr(cardFullPath.find_last_of('/') + 1);
			if (getOneLineFileContent(cardFullPath, cw) != getOneLineFileContent(cardFullPath, mw)) {
				ret = "Width on " + currentBridge + " downgraded to x" + getOneLineFileContent(cardFullPath, cw) + ".";
			}
		}

		while (cardFullPath.back() != '/')
			cardFullPath.pop_back();
		cardFullPath.pop_back();
	}

	return ret;
}

/**
 * @brief check process exclusive
 *
 * This function check process exclusive for a specific process under /proc.
 * @param[in] processId uint32_t values for a specific process ID
 * @return true if the process is ready, false otherwise
 */
bool checkProcessExclusive(uint32_t processId)
{
	std::ifstream file("/proc/" + std::to_string(processId) + "/cmdline");
	if (!file.good()) {
		return false;
	} else {
		return true;
	}
}
