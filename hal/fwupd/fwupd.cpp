#include "fwupd.h"
#include <sys/stat.h>
#include <fstream>

vector<char> fwupd::readImageContent(const char *filePath)
{
	struct stat s;
	if (stat(filePath, &s) != 0 || !(s.st_mode & S_IFREG))
		return std::vector<char>();
	std::ifstream is(std::string(filePath), std::ifstream::binary);
	if (!is)
	{
		return std::vector<char>();
	}
	// get length of file:
	is.seekg(0, is.end);
	int length = (int)is.tellg();
	is.seekg(0, is.beg);

	std::vector<char> buffer(length);

	is.read(buffer.data(), length);
	is.close();
	return buffer;
}

ze_result_t fwupd::updateFW(firmwareInfo *fwInfo)
{
	ze_result_t result = ZE_RESULT_SUCCESS;

	// read image file
	fwInfo->buffer = readImageContent(fwInfo->filePath.c_str());

	result = zesFirmwareFlash(fwInfo->firmwareHandle, fwInfo->buffer.data(), (uint32_t) fwInfo->buffer.size());
	if (result != ZE_RESULT_SUCCESS)
	{
		ERR("Failed to flash firmware: 0x%X (%s)\n", result, l0_error_to_string(result));
		return result;
	}
	DBG("Firmware flash successful.\n");
	return ZE_RESULT_SUCCESS;
};
