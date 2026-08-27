/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_raslog.h"
#include "debug.h"
#include "table_builder.h"
#include <hal.h>
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <format>
#include <fstream>

namespace {

// Strip whitespace-only lines from the top and bottom of a TableBuilder string.
// The default borderless TableBuilder emits a blank top border, a blank header row
// (when columns have empty headers), and a blank bottom border — none of which are
// useful in help text.
std::string trimBorderLines(std::string s)
{
	auto wsOnly = [](std::string_view line) { return line.find_first_not_of(' ') == std::string_view::npos; };
	while (!s.empty()) {
		auto nl = s.find('\n');
		if (nl == std::string::npos || !wsOnly(std::string_view(s).substr(0, nl))) {
			break;
		}
		s.erase(0, nl + 1);
	}
	while (!s.empty() && s.back() == '\n') {
		auto prev = s.rfind('\n', s.size() - 2);
		auto line = prev == std::string::npos ? std::string_view(s).substr(0, s.size() - 1)
											  : std::string_view(s).substr(prev + 1, s.size() - prev - 2);
		if (!wsOnly(line)) {
			break;
		}
		s.resize(prev == std::string::npos ? 0 : prev + 1);
	}
	return s;
}

/**
 * @brief Formats a PCI address as domain:bus:device.function (e.g. "0000:4d:00.0").
 */
std::string formatBdf(const zes_pci_address_t &a)
{
	return std::format("{:04x}:{:02x}:{:02x}.{:x}", a.domain, a.bus, a.device, a.function);
}

/**
 * @brief Formats a ZES UUID as a standard hyphenated lowercase hex string.
 */
std::string formatUuid(const zes_uuid_t &u)
{
	return std::format(
		"{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}", u.id[0],
		u.id[1], u.id[2], u.id[3], u.id[4], u.id[5], u.id[6], u.id[7], u.id[8], u.id[9], u.id[10], u.id[11], u.id[12],
		u.id[13], u.id[14], u.id[15]);
}

std::string_view recordTypeToString(zes_intel_info_log_record_type_exp_t t)
{
	switch (t) {
	case ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_INFORMATIONAL:
		return "INFORMATIONAL";
	case ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_CORRECTED:
		return "ERROR_CORRECTED";
	case ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_RECOVERABLE:
		return "ERROR_RECOVERABLE";
	case ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_FATAL:
		return "ERROR_FATAL";
	default:
		return "UNKNOWN";
	}
}

/**
 * @brief Formats and prints the result of a successful (or partial) CPER collection.
 */
void reportCperSuccess(bool jsonOutput, bool partial, bool peek, size_t byteCount, size_t recordCount,
					   const std::vector<zes_intel_info_log_metadata_exp> &metadata, const std::string &fileName)
{
	if (jsonOutput) {
		nlohmann::ordered_json obj;
		obj["status"] = partial ? "PARTIAL" : "OK";
		obj["bytes"] = byteCount;
		obj["records"] = recordCount;
		obj["file"] = fileName;
		if (partial) {
			obj["warning"] = "Some records were too large for the buffer and were dropped";
		}
		nlohmann::ordered_json recs = nlohmann::ordered_json::array();
		for (const auto &m : metadata) {
			nlohmann::ordered_json rec;
			rec["offset"] = m.offset;
			rec["length"] = m.lengthOfData;
			rec["bdf"] = formatBdf(m.address);
			rec["uuid"] = formatUuid(m.uuid);
			rec["timestamp_ns"] = m.timestamp;
			rec["record_type"] = recordTypeToString(m.recordType);
			recs.push_back(std::move(rec));
		}
		obj["cper_records"] = std::move(recs);
		PRINT("{}\n", obj.dump(4));
	} else if (byteCount == 0) {
		PRINT("CPER buffer is empty (0 bytes, 0 records). Wrote empty file: {}\n", fileName);
	} else if (partial) {
		PRINT("CPER buffer partially {} ({} bytes, {} records, some records dropped). {} to: {}\n",
			  peek ? "peeked" : "read", byteCount, recordCount, peek ? "Copied" : "Written", fileName);
	} else {
		PRINT("CPER buffer ({} bytes, {} records) {} to file: {}\n", byteCount, recordCount,
			  peek ? "peeked (records not consumed)" : "written", fileName);
	}
}

/**
 * @brief Reads (or peeks) the CPER hardware log and writes the raw blob to a file.
 *
 * The output file is validated before any records are read so that a bad path or
 * missing permissions is caught first. Without --peek, reading is destructive:
 * records are consumed from the kernel ring buffer. With --peek, records remain
 * in the buffer for subsequent reads.
 *
 * @param args Command arguments (provides the sysman driver instance).
 * @param fileName File to export the raw CPER blob to.
 * @param jsonOutput Whether to print status in JSON format.
 * @param peek If true, read without consuming records (requires driver isPeekSupported).
 * @retval ZE_RESULT_SUCCESS
 * @retval ZE_RESULT_WARNING_DROPPED_DATA some records were too large for the buffer
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE CPER not supported, or peek not supported on this system
 * @retval ZE_RESULT_ERROR_UNKNOWN  output file could not be opened or written
 */
ze_result_t runCper(arg_struct *args, const std::string &fileName, bool jsonOutput, const std::string &instance,
					CperBufferSizeKb bufferSizeKb, bool peek)
{
	TRACING();

	// Validate the output path is writable before draining — non-truncating open so
	// existing file content survives if the CPER read fails.  The real write (with
	// truncation) happens after we hold the records in memory.
	// ios::app may create an empty file; track that so we can remove the artifact if
	// the drain fails (avoids leaving an empty file that looks like a successful run).
	std::error_code fsEc;
	const bool filePreExisted = std::filesystem::exists(fileName, fsEc);
	{
		std::ofstream check(fileName, std::ios::binary | std::ios::app);
		if (!check.is_open()) {
			if (jsonOutput) {
				nlohmann::ordered_json obj;
				obj["ze_result"] = static_cast<int>(ZE_RESULT_ERROR_UNKNOWN);
				obj["error"] = "Cannot open output file";
				obj["file"] = fileName;
				PRINT("{}\n", obj.dump(4));
			} else {
				ERR("Cannot open output file for writing: {}\n", fileName);
			}
			return ZE_RESULT_ERROR_UNKNOWN;
		}
	}

	std::vector<uint8_t> cperBlob;
	std::vector<zes_intel_info_log_metadata_exp> metadata;
	ze_result_t result = args->sm.getCperLogWithMetadata(
		cperBlob, metadata, instance.empty() ? std::nullopt : std::optional<std::string_view>{instance}, bufferSizeKb,
		peek);

	const bool partial = (result == ZE_RESULT_WARNING_DROPPED_DATA);
	if (result != ZE_RESULT_SUCCESS && !partial) {
		if (!filePreExisted) {
			std::filesystem::remove(fileName, fsEc); // best-effort: remove empty artifact
		}
		const std::string_view errorMsg = [&]() -> std::string_view {
			if (result != ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
				return "Error reading CPER buffer";
			}
			return peek ? "CPER log is not available or peek is not supported on this system"
						: "CPER hardware log is not supported on this system or build";
		}();
		if (jsonOutput) {
			nlohmann::ordered_json obj;
			obj["ze_result"] = static_cast<int>(result);
			obj["error"] = errorMsg;
			PRINT("{}\n", obj.dump(4));
		} else {
			ERR("{}: 0x{:X} ({})\n", errorMsg, result, hal::resultToString(result));
		}
		return result;
	}

	// Open for writing only now that we have records in memory.
	std::ofstream out(fileName, std::ios::binary | std::ios::trunc);
	if (!cperBlob.empty()) {
		out.write(reinterpret_cast<const char *>(cperBlob.data()), static_cast<std::streamsize>(cperBlob.size()));
	}
	out.close();
	if (!out.good()) {
		if (jsonOutput) {
			nlohmann::ordered_json obj;
			obj["ze_result"] = static_cast<int>(ZE_RESULT_ERROR_UNKNOWN);
			obj["error"] = "Failed to write file";
			obj["file"] = fileName;
			PRINT("{}\n", obj.dump(4));
		} else {
			ERR("Failed to write CPER buffer to file: {}\n", fileName);
		}
		return ZE_RESULT_ERROR_UNKNOWN;
	}

	reportCperSuccess(jsonOutput, partial, peek, cperBlob.size(), metadata.size(), metadata, fileName);
	return result;
}

} // namespace

/**
 * @brief Prints the help for the raslog subcommand.
 *
 * @param helpType The verbosity of the help output.
 */
void cmdRasLog::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.emplace_back(TITLE, "Collect GPU hardware RAS error records (CPER)");
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Usage: %s raslog [Options]", progName.c_str());
	helpList.emplace_back(HEADING, "%s raslog -t cper -f [fileName]", progName.c_str());
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Options:");

	printHelp(helpList, helpType);

	if (helpType != FULL_HELP) {
		return;
	}

	// Two-column table: option flag | description.
	// Auto-sizes the flag column; wraps the description to fit an 80-column terminal.
	constexpr int descriptionColumnWidth = 55;
	TableBuilder opts;
	opts.addColumn("").addColumn("");
	opts.setMaxCellWidth(descriptionColumnWidth)
		.enableWordWrap()
		.setColumnWrap(0, false)
		.setColumnWrap(1, true)
		.suppressHeaderSeparator()
		.suppressHeaderColumnSeparators()
		.suppressDataColumnSeparators();

	opts.addRow("-h, --help", "Print this help message and exit")
		.addRow("-j, --json", "Print result in JSON format")
		.addRow("-t, --type",
				"The hardware log type to collect: cper (Common Platform Error Record). Defaults to cper.")
		.addRow("-f, --file", "The file to write the raw hardware log blob into. Reading a hardware log drains the "
							  "buffer, so a file is required to persist the records.")
		.addRow("--peek", "Read records without consuming them. Records remain in the buffer for subsequent reads. "
						  "Requires driver support (isPeekSupported). Returns an error if unsupported.")
		.addRow("--instance",
				"Named tracefs instance to collect from instead of the global trace buffer. Requires driver support.")
		.addRow("--buffer-size-kb", "Total tracefs ring-buffer size in kilobytes across all per-CPU buffers. "
									"The driver splits and rounds to the nearest supported size.");

	PRINT("{}", trimBorderLines(opts.asTable()));
}

/**
 * @brief Executes the raslog subcommand.
 *
 * @retval ZE_RESULT_SUCCESS
 * @retval ZE_RESULT_WARNING_DROPPED_DATA some records were too large for the buffer
 * @retval ZE_RESULT_ERROR_INVALID_ARGUMENT unknown --type or missing --file
 * @retval ZE_RESULT_ERROR_UNSUPPORTED_FEATURE CPER not available on this system
 * @retval ZE_RESULT_ERROR_UNKNOWN file open or write failure
 */
int cmdRasLog::run(arg_struct *args)
{
	TRACING();
	std::string fileName;
	std::string type = "cper";
	std::string instance;
	uint32_t bufferSizeKbVal = 0;
	bool jsonOutput = false;
	bool peek = false;

	CLI::App sub{"Collect GPU hardware RAS error records", "raslog"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", jsonOutput, "Print result in JSON format");
	sub.add_option("-t,--type", type, "The hardware log type to collect (cper)");
	sub.add_option("-f,--file", fileName, "The file to write the raw hardware log blob into");
	sub.add_flag("--peek", peek, "Read records without consuming them (requires driver isPeekSupported)");
	sub.add_option("--instance", instance, "Named tracefs instance (default: global buffer)");
	sub.add_option("--buffer-size-kb", bufferSizeKbVal, "Total tracefs ring-buffer size in kilobytes")
		->check(CLI::PositiveNumber);

	try {
		sub.parse(args->argc - 1, args->argv + 1);
	} catch (const CLI::CallForHelp &) {
		help();
		return ZE_RESULT_SUCCESS;
	} catch (const CLI::ParseError &e) {
		ERR("{}\n", e.what());
		ERR("Run with --help for more information.\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	// Reading a hardware log is destructive, so require -f to avoid draining
	// the records without persisting them.
	if (fileName.empty()) {
		ERR("raslog requires -f/--file: reading drains the hardware log buffer\n");
		help();
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	CperBufferSizeKb bufferSizeKb = (bufferSizeKbVal > 0) ? CperBufferSizeKb{bufferSizeKbVal} : std::nullopt;

	DBG("type: {}, fileName: {}, instance: {}, bufferSizeKb: {}, jsonOutput: {}, peek: {}", type, fileName,
		instance.empty() ? "(global)" : instance, bufferSizeKb ? std::to_string(*bufferSizeKb) : "(default)",
		jsonOutput, peek);

	if (type == "cper") {
		return runCper(args, fileName, jsonOutput, instance, bufferSizeKb, peek);
	}

	ERR("Unsupported hardware log type: {}. Supported types: cper\n", type);
	help();
	return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}
