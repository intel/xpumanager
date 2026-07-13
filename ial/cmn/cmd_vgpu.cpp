/*
 * Copyright (C) 2025-2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_vgpu.h"
#include "debug.h"
#include "printer.h"
#include <CLI/CLI.hpp>
#include "table_builder.h"
#include <osvf.h>
#include <algorithm>
#include <cassert>
#include "utility/compat/format.h"
#include <stdexcept>
#include <unordered_map>

static std::unordered_map<vgpuCmdType, vgpuCmdStruct> vgpuCmds = {
	// NOLINT(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables,misc-use-anonymous-namespace)
	{VGPU_HELP, {.canRunOnIGPU = true}},
	{VGPU_JSON, {.canRunOnIGPU = true}},
	{VGPU_DEVICE, {.canRunOnIGPU = true}},
	{VGPU_PRECHECK, {.func = &cmdVgpu::precheck, .canRunOnIGPU = true}},
	{VGPU_NUMBER, {.canRunOnIGPU = true}},
	{VGPU_CREATE, {.func = &cmdVgpu::create, .canRunOnIGPU = true}},
	{VGPU_REMOVE, {.func = &cmdVgpu::remove, .canRunOnIGPU = true}},
	{VGPU_LIST, {.func = &cmdVgpu::listGpus, .canRunOnIGPU = true}},
	{VGPU_STATS, {.func = &cmdVgpu::stats, .canRunOnIGPU = true}},
	{VGPU_LMEM, {.canRunOnIGPU = false}},
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

	helpList.emplace_back(TITLE, "Create and remove virtual GPUs in SRIOV configuration");
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Usage: %s vgpu [Options]", progName.c_str());
	helpList.emplace_back(HEADING, "%s vgpu --precheck", progName.c_str());
	helpList.emplace_back(HEADING, "%s vgpu --device [deviceId] -c -n [vGpuNumber] --lmem [vGpuMemorySize]",
						  progName.c_str());
	helpList.emplace_back(HEADING,
						  "%s vgpu --device [pciBdfAddress] -c -n [vGpuNumber] "
						  "--lmem [vGpuMemorySize]",
						  progName.c_str());
	helpList.emplace_back(HEADING, "%s vgpu --device [deviceId] -r", progName.c_str());
	helpList.emplace_back(HEADING, "%s vgpu --device [pciBdfAddress] -r", progName.c_str());
	helpList.emplace_back(HEADING, "%s vgpu --device [deviceId] -l", progName.c_str());
	helpList.emplace_back(HEADING, "%s vgpu --device [pciBdfAddress] -l", progName.c_str());
	helpList.emplace_back(HEADING, "%s vgpu --device [deviceId] -s", progName.c_str());
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Options:");
	helpList.emplace_back(HEADING, "-h,--help                   Print this help message and exit");
	helpList.emplace_back(HEADING, "-j,--json                   Print result in JSON format");
	helpList.emplace_back(BLANK);
	helpList.emplace_back(HEADING, "--device,--id               Device ID or PCI BDF address");
	helpList.emplace_back(HEADING, "--precheck,--query          Check if BIOS "
								   "settings are ready to create virtual GPUs");
	helpList.emplace_back(HEADING, "-c,--create                 Create the virtual GPUs");
	helpList.emplace_back(HEADING, "-n                          The number of virtual GPUs to create");
	helpList.emplace_back(HEADING, "--lmem                      Memory size per virtual GPU, in MiB "
								   "(discrete GPUs only). Example: --lmem 500");
	helpList.emplace_back(HEADING, "-r,--remove                 Remove all "
								   "virtual GPUs on the specified physical GPU");
	helpList.emplace_back(HEADING, "-l,--list,--supported       List all virtual "
								   "GPUs on the specified physical GPU");
	helpList.emplace_back(HEADING, "-s,--stats,--utilization    Show statistics data of all virtual GPUs");

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

	bool vmxOk = v->vmxSupport();
	bool sriovOk = v->sriovSupport(&deviceInfo);
	bool iommuOk = v->iommuSupport();

	nlohmann::ordered_json result;
	result["vmx_flag"] = vmxOk ? "Pass" : "Fail";
	result["vmx_message"] = vmxOk ? "" : "No VMX/SVM flag. Please ensure Intel VT or AMD-V is enabled in BIOS";
	result["sriov_status"] = sriovOk ? "Pass" : "Fail";
	result["sriov_message"] = sriovOk ? ""
									  : "SR-IOV is disabled or sriov_totalvfs is 0. Please set the related BIOS "
										"settings and kernel command line parameters.";
	result["iommu_status"] = iommuOk ? "Pass" : "Fail";
	result["iommu_message"] =
		iommuOk ? "" : "IOMMU is disabled. Please set the related BIOS settings and kernel command line parameters.";

	if (vgpuCmds[vgpuCmdType::VGPU_JSON].enabled) {
		JsonPrinter().print(&result);
	} else {
		TableBuilder table;
		table.addColumn("Property", 12, Align::Left)
			.addColumn("Result", 8, Align::Left)
			.addColumn("Message", 80, Align::Left);
		table.addRow("VMX Flag", result["vmx_flag"].get<std::string>(), result["vmx_message"].get<std::string>());
		table.addRow("SR-IOV", result["sriov_status"].get<std::string>(), result["sriov_message"].get<std::string>());
		table.addRow("IOMMU", result["iommu_status"].get<std::string>(), result["iommu_message"].get<std::string>());
		PRINT("{}", table.toString().c_str());
	}
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

	// Check for elevated privileges early
	if (!PRIVILEGECHECK()) {														 // NOLINT(misc-include-cleaner)
		ERR("Creating virtual GPUs requires elevated privileges. Run with sudo.\n"); // NOLINT(misc-include-cleaner)
		return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;							 // NOLINT(misc-include-cleaner)
	}

	ze_result_t result = ZE_RESULT_SUCCESS;
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
			deviceInfo.vGpuMemorySize = static_cast<uint64_t>(stoi(vgpuCmds[vgpuCmdType::VGPU_LMEM].val));
		}
	} catch (std::invalid_argument &) {
		ERR("Error: Invalid argument provided for virtual GPU number or local memory size.\n"); // NOLINT(misc-include-cleaner)
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	} catch (std::out_of_range &) {
		ERR("Error: Argument out of range for virtual GPU number or local memory size.\n"); // NOLINT(misc-include-cleaner)
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// User must provide both vGpuNumber and vGpuMemorySize
	if (deviceInfo.vGpuNumber == 0) {
		ERR("Error: The number of virtual GPUs must be greater than zero.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	const bool isIntegratedGpu = d->dev->isIGPU();
	if (!isIntegratedGpu && deviceInfo.vGpuMemorySize == 0) {
		ERR("Error: The memory size of each virtual GPU must be greater than zero.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Ensure that we aren't executing from a VF
	if (p->getFuncType() == devFuncType::DEVICE_FUNCTION_TYPE_VIRTUAL) {
		ERR("Error: Cannot create virtual GPUs from a virtual function (VF).\n");
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	// Fill in some info in deviceInfo that is needed by lower level functions
	deviceInfo.isIGPU = isIntegratedGpu;
	deviceInfo.bdfAddress = p->getBDFStr();
	deviceInfo.drmPath = d->dev->getDrmDevPath();
	if (e->getState(d->zesDeviceHdl, &eccState) == ZE_RESULT_SUCCESS) {
		deviceInfo.eccState = eccState.currentState;
	}

	// Create the VFs
	result = v->createVFs(&deviceInfo);

	if (result == 0) {
		if (isIntegratedGpu) {
			PRINT("Successfully created {} virtual GPUs on device {}.\n", deviceInfo.vGpuNumber,
				  p->getBDFStr().c_str());
		} else {
			PRINT("Successfully created {} virtual GPUs with {} MiB memory each on device {}.\n", deviceInfo.vGpuNumber,
				  deviceInfo.vGpuMemorySize / ONE_MB_IN_BYTES, p->getBDFStr().c_str());
		}
	} else {
		ERR("Failed to create virtual GPUs on device {}.\n", p->getBDFStr().c_str());
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
ze_result_t cmdVgpu::remove(devInfo *d) // NOLINT(readability-convert-member-functions-to-static)
{
	TRACING();

	// Check for elevated privileges early
	if (!PRIVILEGECHECK()) {														 // NOLINT(misc-include-cleaner)
		ERR("Removing virtual GPUs requires elevated privileges. Run with sudo.\n"); // NOLINT(misc-include-cleaner)
		return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;							 // NOLINT(misc-include-cleaner)
	}

	int result = 0;
	DeviceSriovInfo deviceInfo = {};
	vf *vfHandle = d->dev->getVF();
	pci *pciHandle = d->dev->getPCI();

	deviceInfo.isIGPU = d->dev->isIGPU();
	deviceInfo.bdfAddress = pciHandle->getBDFStr();

	// Remove the VFs
	result = vfHandle->removeVFs(&deviceInfo);
	if (result == 0) {
		PRINT("Successfully removed virtual GPUs on device {}.\n", pciHandle->getBDFStr().c_str());
	} else {
		ERR("Failed to remove virtual GPUs on device {}.\n", pciHandle->getBDFStr().c_str());
	}
	return result == 0 ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

/**
 * @brief Builds the JSON representation of a vGPU (VF) list.
 *
 * @param vfList List of virtual/physical function descriptors to serialize.
 * @return Ordered JSON object of shape {"vgpu_list": [ ... ]}.
 */
nlohmann::ordered_json buildVgpuListJson(const std::vector<DeviceSriovInfo> &vfList)
{
	nlohmann::ordered_json jsonResult;
	jsonResult["vgpu_list"] = nlohmann::ordered_json::array();
	for (const auto &vfInfo : vfList) {
		nlohmann::ordered_json entry;
		entry["bdf_address"] = vfInfo.bdfAddress;
		entry["function_type"] = vfInfo.functionType == DEVICE_FUNCTION_TYPE_VIRTUAL ? "Virtual" : "Physical";
		entry["memory_physical_size_byte"] = vfInfo.vGpuMemorySize;
		jsonResult["vgpu_list"].push_back(entry);
	}
	return jsonResult;
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

	deviceInfo.isIGPU = d->dev->isIGPU();
	deviceInfo.bdfAddress = p->getBDFStr();
	deviceInfo.drmPath = d->dev->getDrmDevPath();

	// List the VFs
	result = v->listVFs(&deviceInfo, vfDeviceInfoList);
	if (result != 0) {
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	if (vgpuCmds[vgpuCmdType::VGPU_JSON].enabled) {
		nlohmann::ordered_json jsonResult = buildVgpuListJson(vfDeviceInfoList);
		JsonPrinter().print(&jsonResult);
	} else {
		for (const auto &vfInfo : vfDeviceInfoList) {
			TableBuilder table;
			table.addColumn("Property", 25, Align::Left).addColumn("Value", 40, Align::Left);

			table.addRow("PCI BDF Address", vfInfo.bdfAddress);
			table.addRow("Function Type", vfInfo.functionType == DEVICE_FUNCTION_TYPE_VIRTUAL ? "Virtual" : "Physical");
			if (vfInfo.vGpuMemorySize > 0) {
				table.addRow("Memory Physical Size",
							 xpum::compat::format("{} MiB", vfInfo.vGpuMemorySize / ONE_MB_IN_BYTES));
			} else {
				table.addRow("Memory Physical Size", "N/A (shared memory / iGPU)");
			}

			PRINT("{}", table.toString().c_str());
		}
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
	deviceInfo.isIGPU = d->dev->isIGPU();
	deviceInfo.bdfAddress = pciHandle->getBDFStr();
	deviceInfo.drmPath = d->dev->getDrmDevPath();

	ze_result_t result = vfHandle->getVFStatsList(&deviceInfo, vfStatsList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to get VF statistics.\n");
		return result;
	}

	if (vfStatsList.empty()) {
		PRINT("No virtual GPUs found on device {}.\n", pciHandle->getBDFStr().c_str());
		return ZE_RESULT_SUCCESS;
	}

	// If all VFs show N/A for engine utilization, hint about privileges
	const bool allEngineNA = std::all_of(vfStatsList.begin(), vfStatsList.end(),
										 [](const VFStatsInfo &s) { return s.gpuUtilization < 0.0; });
	if (allEngineNA && !PRIVILEGECHECK()) {
		ERR("Engine utilization requires elevated privileges. Run with sudo for full stats.\n");
	}

	for (const auto &vfStats : vfStatsList) {
		static constexpr std::size_t bdfStringSize = 16; // "DDDD:BB:DD.F" max length
		char vfBdf
			[bdfStringSize]; // NOLINT(cppcoreguidelines-avoid-c-arrays,hicpp-avoid-c-arrays,modernize-avoid-c-arrays)
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
					 vfStats.gpuUtilization >= 0.0 ? xpum::compat::format("{:.0f}", vfStats.gpuUtilization) : "N/A");
		table.addRow("Compute Engine Util(%)", vfStats.computeUtilization >= 0.0
												   ? xpum::compat::format("{:.0f}", vfStats.computeUtilization)
												   : "N/A");
		table.addRow("Render Engine Util (%)", vfStats.renderUtilization >= 0.0
												   ? xpum::compat::format("{:.0f}", vfStats.renderUtilization)
												   : "N/A");
		table.addRow("Media Engine Util (%)", vfStats.mediaUtilization >= 0.0
												  ? xpum::compat::format("{:.0f}", vfStats.mediaUtilization)
												  : "N/A");
		table.addRow("Copy Engine Util (%)",
					 vfStats.copyUtilization >= 0.0 ? xpum::compat::format("{:.0f}", vfStats.copyUtilization) : "N/A");
		table.addRow("GPU Memory Util (%)", memPercent >= 0.0 ? xpum::compat::format("{:.0f}", memPercent) : "N/A");

		table.addSeparator();

		PRINT("{}", table.toString().c_str());
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

	// Reset state
	for (auto &[k, v] : vgpuCmds) {
		v.enabled = false;
		v.val.clear();
	}

	CLI::App sub{"Manage virtual GPUs (vGPU)", "vgpu"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", vgpuCmds[VGPU_JSON].enabled, "Print result in JSON format");
	sub.add_option("-d,--device,--id", vgpuCmds[VGPU_DEVICE].val,
				   "Device index or BDF address, comma-separated for multiple (e.g. 0,1)")
		->each([&](const std::string &) { vgpuCmds[VGPU_DEVICE].enabled = true; });
	sub.add_flag("--precheck,--query", vgpuCmds[VGPU_PRECHECK].enabled, "Check vGPU readiness");
	sub.add_option("-n,--number", vgpuCmds[VGPU_NUMBER].val, "Number of vGPUs to create")
		->each([&](const std::string &) { vgpuCmds[VGPU_NUMBER].enabled = true; });
	sub.add_flag("-c,--create", vgpuCmds[VGPU_CREATE].enabled, "Create vGPUs");
	sub.add_flag("-r,--remove", vgpuCmds[VGPU_REMOVE].enabled, "Remove vGPUs");
	sub.add_flag("-l,--list,--supported", vgpuCmds[VGPU_LIST].enabled, "List vGPUs");
	sub.add_flag("-s,--stats,--utilization", vgpuCmds[VGPU_STATS].enabled, "Show vGPU stats");
	sub.add_option("--lmem", vgpuCmds[VGPU_LMEM].val, "Local memory size per vGPU (MB)")
		->each([&](const std::string &) { vgpuCmds[VGPU_LMEM].enabled = true; });

	try {
		sub.parse(args->argc - 1, args->argv + 1);
	} catch (const CLI::CallForHelp &) {
		help();
		return ZE_RESULT_SUCCESS;
	} catch (const CLI::ParseError &e) {
		ERR("{}", e.what());
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
		ERR("Error: Device handle not found for device ID '{}'.\n", vgpuCmds[vgpuCmdType::VGPU_DEVICE].val.c_str());
		return result;
	}

	// Iterate through the device list and execute the command
	for (auto &device : deviceList) {
		// Gate unsupported command options on iGPU using per-command capability metadata.
		for (const auto &cmd : vgpuCmds) {
			if (cmd.second.enabled && !cmd.second.canRunOnIGPU && device.dev->isIGPU()) {
				ERR("Command option is not supported for integrated GPU device id: {}\n", device.index);
				return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
			}
		}

		// Call the appropriate command function based on the command type
		for (const auto &cmd : vgpuCmds) {
			if (cmd.second.enabled && cmd.second.func != nullptr) {
				DBG("Running command: {}\n", vgpuCmdName(cmd.first));
				result = (this->*cmd.second.func)(&device);
				break;
			}
		}
	}
	return result;
}
