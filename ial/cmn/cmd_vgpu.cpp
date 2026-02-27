/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_vgpu.h"
#include "debug.h"
#include "table_builder.h"
#include <osvf.h>
#include <algorithm>
#include <assert.h>
#include <cinttypes>

static std::unordered_map<vgpuCmdType, vgpuCmdStruct> vgpuCmds = {
	{VGPU_HELP, {{"help", no_argument, 0, 'h'}, nullptr, false, ""}},
	{VGPU_JSON, {{"json", no_argument, 0, 'j'}, nullptr, false, ""}},
	{VGPU_DEVICE, {{"device", required_argument, 0, 'd'}, nullptr, false, ""}},
	{VGPU_PRECHECK, {{"precheck", no_argument, 0, 0}, &cmdVgpu::precheck, false, ""}},
	{VGPU_NUMBER, {{"number", required_argument, 0, 'n'}, nullptr, false, ""}},
	{VGPU_CREATE, {{"create", no_argument, 0, 'c'}, &cmdVgpu::create, false, ""}},
	{VGPU_REMOVE, {{"remove", no_argument, 0, 'r'}, &cmdVgpu::remove, false, ""}},
	{VGPU_LIST, {{"list", no_argument, 0, 'l'}, &cmdVgpu::listGpus, false, ""}},
	{VGPU_STATS, {{"stats", no_argument, 0, 's'}, &cmdVgpu::stats, false, ""}},
	{VGPU_LMEM, {{"lmem", required_argument, 0, 0}, nullptr, false, ""}},
};

/**
 * @brief Adds help commands to the provided help list.
 *
 * @param helpList A pointer to a list of help commands.
 */
void cmdVgpu::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Create and remove virtual GPUs in SRIOV configuration"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s vgpu [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s vgpu --precheck", progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s vgpu -d [deviceId] -c -n [vGpuNumber] --lmem [vGpuMemorySize]", progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s vgpu -d [pciBdfAddress] -c -n [vGpuNumber] --lmem [vGpuMemorySize]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s vgpu -d [deviceId] -r", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s vgpu -d [pciBdfAddress] -r", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s vgpu -d [deviceId] -l", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s vgpu -d [pciBdfAddress] -l", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s vgpu -d [deviceId] -s", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(HEADING, "-d,--device                 Device ID or PCI BDF address"));
	helpList.push_back(
		helpCmd(HEADING, "--precheck                  Check if BIOS settings are ready to create virtual GPUs"));
	helpList.push_back(helpCmd(HEADING, "-c,--create                 Create the virtual GPUs"));
	helpList.push_back(helpCmd(HEADING, "-n                          The number of virtual GPUs to create"));
	helpList.push_back(helpCmd(
		HEADING, "--lmem                      The memory size of each virtual GPUs, in MiB. For example, --lmem 500"));
	helpList.push_back(
		helpCmd(HEADING, "-r,--remove                 Remove all virtual GPUs on the specified physical GPU"));
	helpList.push_back(
		helpCmd(HEADING, "-l,--list                   List all virtual GPUs on the specified phytsical GPU"));
	helpList.push_back(helpCmd(
		HEADING, "-y,--assumeyes              Assume that the answer to any question which would be asked is yes"));
	helpList.push_back(helpCmd(HEADING, "-s,--stats                  Show statistics data of all virtual GPUs"));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Performs pre-check validation for vGPU operations
 *
 * This function validates system requirements and prerequisites for vGPU functionality.
 * Currently implemented as a placeholder for future vGPU validation logic.
 *
 * @param d[in] Pointer to device information structure
 * @return ze_result_t ZE_RESULT_SUCCESS on successful pre-check validation
 */
ze_result_t cmdVgpu::precheck(devInfo *d)
{
	TRACING();
	vf *v = d->dev->getVF();
	DeviceSriovInfo deviceInfo = {};
	pci *p = d->dev->getPCI();

	deviceInfo.bdfAddress = p->getBDFStr();
	deviceInfo.drmPath = d->dev->getDrmDevPath();

	PRINT("VMX is %s\n", v->vmxSupport() ? "supported" : "not supported");
	PRINT("IOMMU is %s\n", v->iommuSupport() ? "supported" : "not supported");
	PRINT("SR-IOV %s supported on device\n", v->sriovSupport(&deviceInfo) ? "is" : "is not");
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Creates a new virtual GPU instance
 *
 * This function implements the logic to create a new vGPU instance on the specified device.
 * Currently implemented as a placeholder for future vGPU creation functionality.
 *
 * @param d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful vGPU creation
 */
ze_result_t cmdVgpu::create(devInfo *d)
{
	TRACING();

	ze_result_t result;
	DeviceSriovInfo deviceInfo = {};
	zes_device_ecc_properties_t eccState = {};
	pci *p = d->dev->getPCI();
	vf *v = d->dev->getVF();
	ecc *e = d->dev->getECC();

	try {
		if (vgpuCmds[vgpuCmdType::VGPU_NUMBER].enabled) {
			deviceInfo.vGpuNumber = stoi(vgpuCmds[vgpuCmdType::VGPU_NUMBER].val);
		}

		if (vgpuCmds[vgpuCmdType::VGPU_LMEM].enabled) {
			deviceInfo.vGpuMemorySize = stoi(vgpuCmds[vgpuCmdType::VGPU_LMEM].val);
		}
	} catch (std::invalid_argument &) {
		ERR("Error: Invalid argument provided for virtual GPU number.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// User must provide both vGpuNumber and vGpuMemorySize
	if (deviceInfo.vGpuNumber == 0) {
		ERR("Error: The number of virtual GPUs must be greater than zero.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (deviceInfo.vGpuMemorySize == 0) {
		ERR("Error: The memory size of each virtual GPU must be greater than zero.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Ensure that we aren't executing from a VF
	if (p->getFuncType() == devFuncType::DEVICE_FUNCTION_TYPE_VIRTUAL) {
		ERR("Error: Cannot create virtual GPUs from a virtual function (VF).\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	// Fill in some info in deviceInfo that is needed by lower level functions
	deviceInfo.bdfAddress = p->getBDFStr();
	deviceInfo.drmPath = d->dev->getDrmDevPath();
	if (e->getState(d->zesDeviceHdl, &eccState) == ZE_RESULT_SUCCESS) {
		deviceInfo.eccState = eccState.currentState;
	}

	// Create the VFs
	result = v->createVFs(&deviceInfo);

	if (result == 0) {
		PRINT("Successfully created %u virtual GPUs with %" PRIu64 " MiB memory each on device %s.\n",
			  deviceInfo.vGpuNumber, deviceInfo.vGpuMemorySize / ONE_MB_IN_BYTES, p->getBDFStr().c_str());
	} else {
		ERR("Failed to create virtual GPUs on device %s.\n", p->getBDFStr().c_str());
	}

	return result == 0 ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

/**
 * @brief Removes an existing virtual GPU instance
 *
 * This function implements the logic to remove an existing vGPU instance.
 * Currently implemented as a placeholder for future vGPU removal functionality.
 *
 * @param[in] d Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful vGPU removal
 */
ze_result_t cmdVgpu::remove(devInfo *d)
{
	TRACING();
	int result;
	DeviceSriovInfo deviceInfo = {};
	vf *vfHandle = d->dev->getVF();
	pci *pciHandle = d->dev->getPCI();

	deviceInfo.bdfAddress = pciHandle->getBDFStr();
	deviceInfo.drmPath = d->dev->getDrmDevPath();

	// Remove the VFs
	result = vfHandle->removeVFs(&deviceInfo);
	if (result == 0) {
		PRINT("Successfully removed virtual GPUs on device %s.\n", pciHandle->getBDFStr().c_str());
	} else {
		ERR("Failed to remove virtual GPUs on device %s.\n", pciHandle->getBDFStr().c_str());
	}
	return result == 0 ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

/**
 * @brief Lists all available GPU devices for vGPU creation
 *
 * This function retrieves and displays all GPU devices available for vGPU operations.
 * Currently implemented as a placeholder for future GPU device listing functionality.
 *
 * @param d[in] Pointer to device information structure (currently unused)
 * @return ze_result_t ZE_RESULT_SUCCESS on successful device listing
 */
ze_result_t cmdVgpu::listGpus(devInfo *d)
{
	TRACING();
	int result;
	DeviceSriovInfo deviceInfo = {};
	std::vector<DeviceSriovInfo> vfDeviceInfoList;
	vf *v = d->dev->getVF();
	pci *p = d->dev->getPCI();

	deviceInfo.bdfAddress = p->getBDFStr();
	deviceInfo.drmPath = d->dev->getDrmDevPath();

	// List the VFs
	result = v->listVFs(&deviceInfo, vfDeviceInfoList);
	if (result != 0) {
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	for (const auto &vfInfo : vfDeviceInfoList) {
		TableBuilder table;
		table.addColumn("Property", 25, Align::Left).addColumn("Value", 40, Align::Left);

		table.addRow("PCI BDF Address", vfInfo.bdfAddress);
		table.addRow("Function Type", vfInfo.functionType == DEVICE_FUNCTION_TYPE_VIRTUAL ? "Virtual" : "Physical");
		table.addRow("Memory Physical Size", std::format("{} MiB", vfInfo.vGpuMemorySize / ONE_MB_IN_BYTES));

		PRINT("%s", table.toString().c_str());
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Retrieves statistics for virtual GPU instances
 *
 * This function collects and displays performance statistics for vGPU instances,
 * matching the legacy xpum-cli output format with utilization percentages.
 *
 * @param d Pointer to device information structure
 * @return ze_result_t ZE_RESULT_SUCCESS on successful statistics retrieval
 */
ze_result_t cmdVgpu::stats(devInfo *d)
{
	TRACING();
	std::vector<VFStatsInfo> vfStatsList;
	vf *vfHandle = d->dev->getVF();
	pci *pciHandle = d->dev->getPCI();

	DeviceSriovInfo deviceInfo = {};
	deviceInfo.bdfAddress = pciHandle->getBDFStr();
	deviceInfo.drmPath = d->dev->getDrmDevPath();

	ze_result_t result = vfHandle->getVFStatsList(&deviceInfo, vfStatsList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get VF statistics.\n");
		return result;
	}

	if (vfStatsList.empty()) {
		PRINT("No virtual GPUs found on device %s.\n", pciHandle->getBDFStr().c_str());
		return ZE_RESULT_SUCCESS;
	}

	for (const auto &vfStats : vfStatsList) {
		static constexpr size_t bdfStringSize = 16; // "DDDD:BB:DD.F" max length
		char vfBdf[bdfStringSize];
		snprintf(vfBdf, sizeof(vfBdf), "%04x:%02x:%02x.%x", vfStats.domain, vfStats.bus, vfStats.device,
				 vfStats.function);

		double memPercent = -1.0;
		if (vfStats.vfDeviceMemSize > 0) {
			memPercent =
				(static_cast<double>(vfStats.memoryUtilized) / static_cast<double>(vfStats.vfDeviceMemSize)) * 100.0;
			memPercent = std::min(memPercent, 100.0);
		}

		TableBuilder table;
		table.addColumn("Metric", 26, Align::Left).addColumn("Value", 67, Align::Left);

		table.addSeparator();
		table.addSpanRow("Device Information");
		table.addSeparator();

		table.addRow("PCI BDF Address", vfBdf);
		table.addRow("GPU Utilization (%)",
					 vfStats.gpuUtilization >= 0.0 ? std::format("{:.0f}", vfStats.gpuUtilization) : "N/A");
		table.addRow("Compute Engine Util(%)",
					 vfStats.computeUtilization >= 0.0 ? std::format("{:.0f}", vfStats.computeUtilization) : "N/A");
		table.addRow("Render Engine Util (%)",
					 vfStats.renderUtilization >= 0.0 ? std::format("{:.0f}", vfStats.renderUtilization) : "N/A");
		table.addRow("Media Engine Util (%)",
					 vfStats.mediaUtilization >= 0.0 ? std::format("{:.0f}", vfStats.mediaUtilization) : "N/A");
		table.addRow("Copy Engine Util (%)",
					 vfStats.copyUtilization >= 0.0 ? std::format("{:.0f}", vfStats.copyUtilization) : "N/A");
		table.addRow("GPU Memory Util (%)", memPercent >= 0.0 ? std::format("{:.0f}", memPercent) : "N/A");

		table.addSeparator();

		PRINT("%s", table.toString().c_str());
	}

	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Executes the vgpu run.
 *
 * @return int Returns 0 on success.
 */
int cmdVgpu::run(arg_struct *args)
{
	TRACING();
	std::vector<devInfo> deviceList;
	ze_result_t result;
	bool found = false;
	int opt;
	int optionIndex = 0;
	std::string shortOpts;
	std::vector<struct option> longOptsVec;

	processOptions(vgpuCmds, shortOpts, longOptsVec);
	const struct option *longOpts = longOptsVec.data();
	// Skip the first two arguments (process and command name)
	int startind = 2;
	optind = 2;

	while ((opt = getopt_long(args->argc, args->argv, shortOpts.c_str(), longOpts, &optionIndex)) != -1) {
		switch (opt) {
		case 'h':
			help();
			return ZE_RESULT_SUCCESS;
		case 'j':
			vgpuCmds[vgpuCmdType::VGPU_JSON].enabled = true;
			break;
		case 'd':
			vgpuCmds[vgpuCmdType::VGPU_DEVICE].val = optarg;
			vgpuCmds[vgpuCmdType::VGPU_DEVICE].enabled = true;
			break;
		case 'c':
			vgpuCmds[vgpuCmdType::VGPU_CREATE].enabled = true;
			break;
		case 'r':
			vgpuCmds[vgpuCmdType::VGPU_REMOVE].enabled = true;
			break;
		case 'l':
			vgpuCmds[vgpuCmdType::VGPU_LIST].enabled = true;
			break;
		case 'n':
			vgpuCmds[vgpuCmdType::VGPU_NUMBER].enabled = true;
			vgpuCmds[vgpuCmdType::VGPU_NUMBER].val = optarg;
			break;
		case 's':
			vgpuCmds[vgpuCmdType::VGPU_STATS].enabled = true;
			break;
		case 0:
			for (auto &cmd : vgpuCmds) {
				if (STRCASECMP(longOpts[optionIndex].name, cmd.second.opt.name) == 0) {
					cmd.second.enabled = true;
					if (longOpts[optionIndex].has_arg == required_argument) {
						cmd.second.val = optarg;
					}
					found = true;
					break;
				}
			}

			if (!found) {
				ERR("The following argument was not expected: '%s'.\n", longOpts[optionIndex].name);
				ERR("Run with --help for more information.\n");
				return ZE_RESULT_ERROR_INVALID_ARGUMENT;
			}

			break;
		default:
			ERR("The following argument was not expected: '%s'.\n", args->argv[startind]);
			ERR("Run with --help for more information.\n");
			break;
		}
		startind++;
	}

	// If optind is not equal to args->argc, it means there are extra arguments
	// that were not processed by getopt_long.
	if (optind != args->argc) {
		ERR("The following argument was not expected: '%s'.\n", args->argv[optind]);
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// If create, delete or list commands are provided, then the user must also provide a device as well
	if ((vgpuCmds[vgpuCmdType::VGPU_CREATE].enabled || vgpuCmds[vgpuCmdType::VGPU_REMOVE].enabled ||
		 vgpuCmds[vgpuCmdType::VGPU_LIST].enabled) &&
		!vgpuCmds[vgpuCmdType::VGPU_DEVICE].enabled) {
		ERR("Error: Create/Remove/List require a device to be provided as a command line option.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	result = args->sm.findDevice(vgpuCmds[vgpuCmdType::VGPU_DEVICE].val.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '%s'.\n", vgpuCmds[vgpuCmdType::VGPU_DEVICE].val.c_str());
		return result;
	}

	// Iterate through the device list and execute the command
	for (auto &device : deviceList) {
		// Call the appropriate command function based on the command type
		for (const auto &cmd : vgpuCmds) {
			if (cmd.second.enabled && cmd.second.func != nullptr) {
				DBG("Running command: %s\n", cmd.second.opt.name);
				result = (this->*cmd.second.func)(&device);
				break;
			}
		}
	}
	return result;
}