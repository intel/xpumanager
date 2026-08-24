/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_crashlog.h"
#include "bdf.h"
#include "debug.h"
#include "printer.h"
#include <CLI/CLI.hpp>
#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <system_error>
#include <vector>

// The Intel Crash Log project, shown when its CLI is unavailable.
static constexpr const char *ICLG_PROJECT_URL = "https://github.com/intel/crashlog";

/**
 * @brief Construct the crashlog text printer.
 */
CrashlogTextPrinter::CrashlogTextPrinter() : TextPrinter() {}

/**
 * @brief Render the per-device crashlog result array as human-readable lines.
 *
 * @param jsonObj The result object built by cmdCrashlog::run; ignored if it is
 *                null or has no "crashlog" array.
 */
void CrashlogTextPrinter::print(nlohmann::ordered_json *jsonObj)
{
	if (jsonObj == nullptr || !jsonObj->contains("crashlog")) {
		return;
	}
	for (const auto &e : (*jsonObj)["crashlog"]) {
		const uint32_t id = e.value("device_id", 0U);
		const std::string bdf = e.value("pci_bdf_address", std::string{});
		const std::string action = e.value("action", std::string{});
		const std::string status = e.value("status", std::string{});
		PRINT("Device {} ({}): {} - {}\n", id, bdf.c_str(), action.c_str(), status.c_str());
		if (e.contains("extracted_files")) {
			for (const auto &f : e["extracted_files"]) {
				PRINT("  Extracted: {}\n", f.get<std::string>().c_str());
			}
		}
		if (e.contains("decoded_files")) {
			for (const auto &f : e["decoded_files"]) {
				PRINT("  Decoded:   {}\n", f.get<std::string>().c_str());
			}
		}
		if (e.contains("error")) {
			PRINT("  Error: {}\n", e["error"].get<std::string>().c_str());
		}
	}
}

/**
 * @brief Map a crashlog action to the iclg subcommand verb it invokes.
 *
 * @param action The requested crashlog action.
 * @return The iclg verb (e.g. "enable"), or "" for Action::None.
 */
const char *cmdCrashlog::verbForAction(Action action)
{
	switch (action) {
	case Action::Enable:
		return "enable";
	case Action::Disable:
		return "disable";
	case Action::Trigger:
		return "trigger";
	case Action::Clear:
		return "clear";
	case Action::Extract:
		return "extract";
	case Action::None:
	default:
		return "";
	}
}

/**
 * @brief Print the crashlog command usage and option help.
 *
 * @param helpType Level of detail to print (defaults to full help).
 */
void cmdCrashlog::help(HELP helpType)
{
	std::vector<helpCmd> helpList;

	helpList.push_back(helpCmd(TITLE, "Intel Crash Log Management"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Usage: %s crashlog [Options]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s crashlog --enable  -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s crashlog --disable -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s crashlog --trigger -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s crashlog --clear   -d [deviceId]", progName.c_str()));
	helpList.push_back(helpCmd(HEADING, "%s crashlog --extract -d [deviceId] --output [dir]", progName.c_str()));
	helpList.push_back(
		helpCmd(HEADING, "%s crashlog --extract -d [deviceId] --output [dir] --decode", progName.c_str()));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Options:"));
	helpList.push_back(helpCmd(HEADING, "-h,--help                   Print this help message and exit"));
	helpList.push_back(helpCmd(HEADING, "--enable                    Enable crash log collection"));
	helpList.push_back(helpCmd(HEADING, "--disable                   Disable crash log collection"));
	helpList.push_back(helpCmd(HEADING, "--trigger                   Trigger an on-demand crash log collection"));
	helpList.push_back(helpCmd(HEADING, "--clear                     Clear the crash log storage"));
	helpList.push_back(helpCmd(HEADING, "--extract                   Extract crash log records to a directory"));
	helpList.push_back(
		helpCmd(HEADING, "--output                    Existing output directory for --extract (default: current dir)"));
	helpList.push_back(
		helpCmd(HEADING, "--decode                    With --extract, also decode each record to JSON (<file>.json)"));
	helpList.push_back(helpCmd(HEADING,
							   "-d,--device,--id            Device index or PCI BDF address, comma-separated for "
							   "multiple (e.g. 0,1)"));
	helpList.push_back(helpCmd(HEADING, "-j,--json                   Print result in JSON format"));
	helpList.push_back(helpCmd(BLANK));
	helpList.push_back(helpCmd(TITLE, "Notes:"));
	helpList.push_back(helpCmd(HEADING, "  - Requires the 'iclg' utility from %s", ICLG_PROJECT_URL));
	helpList.push_back(helpCmd(HEADING, "  - The operation may require root/administrator privileges"));

	printHelp(helpList, helpType);
	helpList.clear();
}

/**
 * @brief Parse arguments and run the requested crash log operation per device.
 *
 * OS-specific work (locating and invoking iclg) is delegated to the driver's
 * crashlog* methods, so this function contains no platform conditionals.
 *
 * @param args Parsed command context, including the driver (args->sm).
 * @return A ze_result_t: success when every selected device succeeded, an
 *         unsupported-feature error when every failure was an unsupported
 *         device, and a general error otherwise.
 */
int cmdCrashlog::run(arg_struct *args)
{
	TRACING();

	if (args->argc == 2) {
		help();
		return ZE_RESULT_SUCCESS;
	}

	bool doEnable = false;
	bool doDisable = false;
	bool doTrigger = false;
	bool doClear = false;
	bool doExtract = false;
	bool doDecode = false;
	bool jsonOut = false;
	std::string deviceSpec;
	std::string outputPath;

	CLI::App sub{"Intel Crash Log management operations", "crashlog"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");

	// The action flags are mutually exclusive and exactly one is required. An
	// option group with require_option(1) enforces both rules during parsing,
	// and keeps doing so as actions are added or removed.
	CLI::Option_group *actions = sub.add_option_group("Action");
	actions->add_flag("--enable", doEnable, "Enable crash log collection");
	actions->add_flag("--disable", doDisable, "Disable crash log collection");
	actions->add_flag("--trigger", doTrigger, "Trigger an on-demand crash log collection");
	actions->add_flag("--clear", doClear, "Clear the crash log storage");
	actions->add_flag("--extract", doExtract, "Extract crash log records to a directory");
	actions->require_option(1);

	sub.add_option("--output", outputPath, "Existing output directory for --extract");
	sub.add_flag("--decode", doDecode, "With --extract, also decode each record to JSON");
	sub.add_option("-d,--device,--id", deviceSpec, "Device index or PCI BDF address, comma-separated for multiple");
	sub.add_flag("-j,--json", jsonOut, "Print result in JSON format");

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

	// require_option(1) above guarantees exactly one action flag is set.
	const Action action = doEnable	  ? Action::Enable
						  : doDisable ? Action::Disable
						  : doTrigger ? Action::Trigger
						  : doClear	  ? Action::Clear
									  : Action::Extract;

	if (!doExtract && (!outputPath.empty() || doDecode)) {
		ERR("Error: --output and --decode are only valid with --extract.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (deviceSpec.empty()) {
		ERR("Error: a device must be specified with -d/--device.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// A single source can emit several records, and iclg auto-names them uniquely
	// only when writing into a directory; given a file path it would overwrite or
	// refuse. So when --output is given it must be an existing directory. The
	// non-throwing overload treats a stat error (e.g. permission denied) as false.
	if (action == Action::Extract && !outputPath.empty()) {
		std::error_code ec;
		if (!std::filesystem::is_directory(outputPath, ec)) {
			ERR("--output must be an existing directory.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	if (!args->sm.crashlogSupported()) {
		ERR("The 'iclg' (Intel Crash Log) utility is not available.\n");
		ERR("On Linux, install it from {} and ensure it is on PATH.\n", ICLG_PROJECT_URL);
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}

	std::vector<devInfo> deviceList;
	const ze_result_t findResult = args->sm.findDevice(deviceSpec.c_str(), &deviceList);
	if (findResult != ZE_RESULT_SUCCESS || deviceList.empty()) {
		ERR("No matching device found for '{}'.\n", deviceSpec.c_str());
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// iclg reports success for a control operation even when the requested
	// source has no crash log endpoints, so exit status alone cannot tell a
	// real operation from a no-op. Query the sources iclg actually sees and
	// reject any device that is not among them.
	std::set<std::string> supportedBdfs;
	std::string listMessage;
	if (args->sm.crashlogListSources(supportedBdfs, listMessage) != ZE_RESULT_SUCCESS) {
		ERR("Failed to query available crash log sources.\n");
		if (!listMessage.empty()) {
			ERR("{}\n", listMessage);
		}
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	const char *const verb = verbForAction(action);
	nlohmann::ordered_json results = nlohmann::ordered_json::array();
	int failures = 0;
	int unsupported = 0;

	for (auto &d : deviceList) {
		nlohmann::ordered_json entry;
		entry["device_id"] = d.index;
		entry["action"] = verb;

		const std::string bdf = (d.dev != nullptr) ? d.dev->getBDFStr() : std::string{};
		entry["pci_bdf_address"] = bdf;
		if (!isValidBdf(bdf)) {
			ERR("Device {}: could not obtain a valid PCI BDF address.\n", d.index);
			entry["status"] = "failed";
			entry["error"] = "invalid PCI BDF address";
			results.push_back(entry);
			failures++;
			continue;
		}

		if (!supportedBdfs.contains(bdf)) {
			ERR("Device {} ({}) is not supported by the crash log feature.\n", d.index, bdf.c_str());
			entry["status"] = "unsupported";
			entry["error"] = "device has no Intel Crash Log (PMT) source";
			results.push_back(entry);
			failures++;
			unsupported++;
			continue;
		}

		std::string message;
		if (action == Action::Extract) {
			std::vector<std::string> files;
			if (args->sm.crashlogExtract(bdf, outputPath, files, message) != ZE_RESULT_SUCCESS || files.empty()) {
				entry["status"] = "failed";
				entry["error"] = message.empty() ? std::string("extract failed") : message;
				results.push_back(entry);
				failures++;
				continue;
			}
			entry["extracted_files"] = files;

			if (doDecode) {
				std::vector<std::string> decoded;
				bool decodeFailed = false;
				for (const std::string &file : files) {
					const std::string jsonPath = file + ".json";
					std::string decodeMessage;
					if (args->sm.crashlogDecode(file, jsonPath, decodeMessage) != ZE_RESULT_SUCCESS) {
						entry["status"] = "decode_failed";
						entry["error"] = decodeMessage.empty() ? std::string("decode failed") : decodeMessage;
						decodeFailed = true;
						break;
					}
					decoded.push_back(jsonPath);
				}
				if (decodeFailed) {
					// Records decoded before the failure were written to disk;
					// report them so a caller can still find that partial output.
					if (!decoded.empty()) {
						entry["decoded_files"] = decoded;
					}
					results.push_back(entry);
					failures++;
					continue;
				}
				entry["decoded_files"] = decoded;
			}
		} else {
			if (args->sm.crashlogControl(verb, bdf, message) != ZE_RESULT_SUCCESS) {
				entry["status"] = "failed";
				if (!message.empty()) {
					entry["error"] = message;
				}
				results.push_back(entry);
				failures++;
				continue;
			}
		}

		entry["status"] = "ok";
		results.push_back(entry);
	}

	nlohmann::ordered_json outputJson;
	outputJson["crashlog"] = results;

	std::unique_ptr<Printer> printer;
	if (jsonOut) {
		printer = std::make_unique<JsonPrinter>();
	} else {
		printer = std::make_unique<CrashlogTextPrinter>();
	}
	printer->print(&outputJson);

	if (failures == 0) {
		return ZE_RESULT_SUCCESS;
	}
	// If every failure was an unsupported device, report that specifically.
	if (unsupported == failures) {
		return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
	}
	return ZE_RESULT_ERROR_UNKNOWN;
}
