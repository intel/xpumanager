/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmd_dump.h"
#include "cmds.h"
#include "logger/logger.h"
#include "device.h"
#include "metrics_registry.h"
#include "table_builder.h"
#include "ze_api.h"
#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <format>
#include <functional>
#include <ios>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <variant>
#include <utility>
#include <vector>

namespace {

// Parsing utilities

/**
 * @brief Parse an integer from a string_view using std::from_chars.
 *
 * @tparam T      Integral type to parse into.
 * @param[in] str Input string; must consist solely of the integer representation
 *                with no leading or trailing whitespace.
 * @retval std::optional<T>  The parsed value on success.
 * @retval std::nullopt      When the string is not a valid integer or contains
 *         unconsumed trailing characters after the number.
 */
template <typename T> [[nodiscard]] std::optional<T> parseInteger(std::string_view str) noexcept
{
	T value{};
	const auto result = std::from_chars(str.data(), str.data() + str.size(), value);
	if (result.ec == std::errc{} && result.ptr == str.data() + str.size()) {
		return value;
	}
	return std::nullopt;
}

/**
 * @brief Extract a named flag and its value from an argv span in a single pass.
 *
 * Supports two syntactic forms:
 *  - @c "--flag=value" / @c "-flag=value"  (equals-delimited)
 *  - @c "--flag value" / @c "-flag value"  (space-separated)
 *
 * @param[in] argv       Span of raw argv pointers (not including @c argv[0]).
 * @param[in] flagShort  Short form of the flag (e.g. @c "-lms"); pass an empty
 *                       string_view to ignore the short form.
 * @param[in] flagLong   Long form of the flag (e.g. @c "--loop-ms"); pass an empty
 *                       string_view to ignore the long form.
 * @return A pair where:
 *   - @c first:  The remaining argument strings with the matched flag and its value removed.
 *   - @c second: The flag's value if found; std::nullopt otherwise.
 */
[[nodiscard]] std::pair<std::vector<std::string>, std::optional<std::string>>
filterArgvAndExtract(std::span<char *const> argv, std::string_view flagShort, std::string_view flagLong)
{
	std::vector<std::string> result;
	std::optional<std::string> extracted;

	for (std::size_t i = 0; i < argv.size(); ++i) {
		const std::string_view arg{argv[i]};
		bool skip = false;

		// Check --flag=value or -flag=value
		for (const auto flag : {flagLong, flagShort}) {
			if (!flag.empty() && arg.starts_with(flag) && arg.size() > flag.size() && arg[flag.size()] == '=') {
				extracted = std::string{arg.substr(flag.size() + 1)};
				skip = true;
				break;
			}
		}
		if (skip) {
			continue;
		}

		// Check --flag value or -flag value (space-separated)
		if (arg == flagLong || arg == flagShort) {
			if (i + 1 < argv.size()) {
				extracted = std::string{argv[++i]};
			}
			continue;
		}

		result.emplace_back(arg);
	}

	return {result, extracted};
}

/**
 * @brief Map from legacy numeric metric ID to canonical registry field name.
 *
 * Preserved for backward compatibility so that @c "dump -m 0,1,2" continues to work.
 * Unsupported legacy IDs (21, 27, 28) are absent from the map and produce an error log
 * entry in translateMetricQuery().
 */
const std::unordered_map<int, std::string_view> LEGACY_METRIC_NAMES = {
	{0, "utilization.gpu"},					// GPU Utilization (%)
	{1, "power.draw"},						// GPU Power (W)
	{2, "clocks.current.graphics"},			// GPU Frequency (MHz)
	{3, "temperature.gpu"},					// GPU Core Temperature (C)
	{4, "temperature.memory"},				// GPU Memory Temperature (C)
	{5, "utilization.memory"},				// GPU Memory Utilization (%)
	{6, "memory.read.bandwidth"},			// GPU Memory Read (kB/s)
	{7, "memory.write.bandwidth"},			// GPU Memory Write (kB/s)
	{8, "energy.consumed"},					// GPU Energy Consumed (J)
	{9, "eu.active"},						// EU Array Active (%)
	{10, "eu.stall"},						// EU Array Stall (%)
	{11, "eu.idle"},						// EU Array Idle (%)
	{12, "ras.reset"},						// Reset Counter
	{13, "ras.programming.errors"},			// Programming Errors
	{14, "ras.driver.errors"},				// Driver Errors
	{15, "ras.cache.errors.correctable"},	// Cache Errors Correctable
	{16, "ras.cache.errors.uncorrectable"}, // Cache Errors Uncorrectable
	{17, "memory.bandwidth.utilization"},	// Memory Bandwidth Utilization (%)
	{18, "memory.used"},					// Memory Used (MiB)
	{19, "pcie.rx.throughput.kbs"},			// PCIe Read (kB/s)
	{20, "pcie.tx.throughput.kbs"},			// PCIe Write (kB/s)
	// 21: Xe Link Throughput -- not supported
	{22, "utilization.compute"}, // Compute Engine Utilization (%)
	{23, "utilization.render"},	 // Render Engine Utilization (%)
	{24, "utilization.media"},	 // Media Decoder Engine Utilization (%)
	{25, "utilization.media"},	 // Media Encoder Engine Utilization (%)
	{26, "utilization.copy"},	 // Copy Engine Utilization (%)
	// 27: Media Enhancement Engine -- not supported
	// 28: 3D Engine Utilization -- not supported
	{29, "ras.non_compute.errors.correctable"},	  // Memory Errors Correctable
	{30, "ras.non_compute.errors.uncorrectable"}, // Memory Errors Uncorrectable
	{31, "utilization.compute"},				  // Compute Engine Group Utilization (%)
	{32, "utilization.render"},					  // Render Engine Group Utilization (%)
	{33, "utilization.media"},					  // Media Engine Group Utilization (%)
	{34, "utilization.copy"},					  // Copy Engine Group Utilization (%)
	{35, "clocks.throttle.reason"},				  // Throttle Reason
	{36, "clocks.current.media"},				  // Media Engine Frequency (MHz)
};

/**
 * @brief Translate a user-supplied metric query string into canonical registry field names.
 *
 * The input is a comma-separated list of tokens, each of which may be:
 *  - A legacy numeric ID (0–36): mapped via LEGACY_METRIC_NAMES.
 *  - Any other token: passed through unchanged for metrics::resolveQuery() to handle
 *    (group names, single- and multi-character shortcuts via parseGroupMask).
 *
 * Single/multi-character shortcuts follow the registry's GROUP_TABLE:
 *  @c p = POWER+TEMPERATURE, @c u = UTILIZATION, @c t = PCI, etc.
 *
 * Unsupported legacy IDs produce an error log entry and are silently dropped.
 *
 * @param[in] query  Comma-separated metric query string supplied by the user.
 * @return           Translated comma-separated query string suitable for metrics::resolveQuery().
 *                   Returns an empty string if all tokens were invalid or the input was empty.
 */
std::string translateMetricQuery(const std::string &query)
{
	std::string result;
	for (auto &&rng : std::string_view{query} | std::views::split(',')) {
		std::string_view sv{rng.begin(), rng.end()};
		const auto b = sv.find_first_not_of(" \t");
		if (b == std::string_view::npos) {
			continue;
		}
		sv = sv.substr(b, sv.find_last_not_of(" \t") - b + 1);

		std::string token;
		if (const auto n = parseInteger<int>(sv); n.has_value()) {
			const auto it = LEGACY_METRIC_NAMES.find(*n);
			if (it == LEGACY_METRIC_NAMES.end()) {
				ERR("Legacy metric {} is not supported; skipping.\n", *n);
				continue;
			}
			token = it->second;
		} else {
			token = std::string{sv}; // group names, shortcuts, and multi-char combos handled by resolveQuery
		}

		if (!result.empty()) {
			result += ',';
		}
		result += token;
	}
	return result;
}

/**
 * @brief Build aligned "Group:" / "Alias:" display lines for the dump help text.
 *
 * Derived from metrics::detail::GROUP_TABLE (excludes IDENTITY). Shortcuts match
 * nvidia-smi dmon conventions: @c p = POWER+TEMPERATURE, @c t = PCI, etc.
 *
 * @return  Vector of non-blank strings, typically two elements (Group row then Alias row).
 */
std::vector<std::string> buildGroupAliasLines()
{
	// Derived from metrics::detail::GROUP_TABLE; IDENTITY excluded (not a dump metric group).
	using Entry = std::pair<std::string_view, std::string_view>;
	std::vector<Entry> groups;
	for (const auto &e : metrics::detail::GROUP_TABLE) {
		if (e.group != metrics::MetricGroup::IDENTITY) {
			groups.emplace_back(e.name, e.shortcut);
		}
	}

	TableBuilder tb;
	tb.configure(TableBuilder::TableConfig{.borderChar = ' ', .cornerChar = ' ', .verticalChar = ' '})
		.suppressHeaderSeparator()
		.suppressHeaderColumnSeparators()
		.suppressDataColumnSeparators()
		.enableAutoSizing();

	// One label column + one column per group (all with empty headers).
	tb.addColumn("", Align::Left);
	for (std::size_t i = 0; i < std::size(groups); ++i) {
		tb.addColumn("", Align::Left);
	}

	{
		std::vector<std::string> row{"Group:"};
		for (const auto &[g, a] : groups) {
			row.emplace_back(g);
		}
		tb.addRowFromContainer(row);
	}
	{
		std::vector<std::string> row{"Alias:"};
		for (const auto &[g, a] : groups) {
			row.emplace_back(a);
		}
		tb.addRowFromContainer(row);
	}

	std::vector<std::string> lines;
	for (const auto &rng : tb.toString() | std::views::split('\n')) {
		const std::string_view line{rng.begin(), rng.end()};
		if (line.find_first_not_of(' ') != std::string_view::npos) {
			lines.emplace_back(line);
		}
	}
	return lines;
}

/**
 * @brief Parsed command-line options for the @c dump / @c dmon command.
 *
 * All @c std::optional fields are absent (@c std::nullopt) when the corresponding
 * flag was not supplied on the command line.
 */
struct DumpOpts
{
	bool json = false;
	bool date = false;
	bool csvFormat = false; /**< --format=csv: force CSV output even on a TTY */
	bool noheader = false;	/**< --format=...,noheader: suppress the header row */
	bool nounits = false;	/**< --format=...,nounits: strip unit suffixes from column headers */
	std::string device;
	std::optional<std::string> metrics;
	std::optional<std::string> file;
	std::optional<std::string> time;
	std::optional<std::string> interval;
	std::optional<std::string> number;
};

/**
 * @brief Resolved sampling-timing parameters produced by parseSamplingTiming().
 *
 * Fields with a sentinel value of @c -1 indicate "unlimited / not specified".
 */
struct SamplingTiming
{
	std::chrono::milliseconds interval{DEFAULT_INTERVAL};
	int64_t totalTimeSeconds = -1; // -1 = unlimited
	int iterations = -1;		   // -1 = unlimited
};

/**
 * @brief Validate, translate, and resolve a --metrics argument.
 *
 * Rejects dot-notation field names that belong to @c --query-gpu, then delegates to
 * translateMetricQuery() followed by metrics::resolveQuery().
 *
 * @pre   @p metricsArg must not be empty.
 * @param[in] metricsArg  Raw value of the --metrics option.
 * @return  Non-owning pointers to the resolved QueryMetric descriptors, in query order.
 *          Returns an empty vector on any validation or resolution failure (error already logged).
 */
std::vector<const metrics::QueryMetric *> resolveMetricsArg(const std::string &metricsArg)
{
	for (auto &&rng : std::string_view{metricsArg} | std::views::split(',')) {
		std::string_view sv{rng.begin(), rng.end()};
		const auto b = sv.find_first_not_of(" \t");
		if (b == std::string_view::npos) {
			continue;
		}
		sv = sv.substr(b, sv.find_last_not_of(" \t") - b + 1);
		if (!parseInteger<int>(sv).has_value() && sv.find('.') != std::string_view::npos) {
			ERR("'{}' looks like a field name. Use '--query-gpu={}' for field-name queries.\n", sv, metricsArg.c_str());
			ERR("Use group names with --metrics (e.g. '--metrics POWER' or '--metrics pu').\n");
			return {};
		}
	}
	const std::string translated = translateMetricQuery(metricsArg);
	if (translated.empty()) {
		return {};
	}
	auto fields = metrics::resolveQuery(translated);
	if (fields.empty()) {
		ERR("No valid metrics matched: '{}'", metricsArg.c_str());
		ERR("Use group names like 'POWER' or 'pu'. Run with --help for details.\n");
	}
	return fields;
}

/**
 * @brief Parse and validate the sampling-timing options from DumpOpts.
 *
 * Validation rules applied in order:
 *  - @c --number: positive integer.
 *  - @c --interval: positive integer, at most MAX_INTERVAL seconds.
 *  - @c --loop-ms (@p loopMs): positive integer in ms; overrides @c --interval when present.
 *  - @c --time: total wall-clock duration in seconds; may not be combined with @c --number.
 *
 * @param[in] opts    Fully-parsed DumpOpts from parseDumpCLI().
 * @param[in] loopMs  Optional value of the @c --loop-ms flag, pre-extracted
 *                    by filterArgvAndExtract() before CLI11 parsing.
 * @retval std::optional<SamplingTiming>  Populated timing struct on success.
 * @retval std::nullopt  If any validation error is detected (error already logged).
 */
std::optional<SamplingTiming> parseSamplingTiming(const DumpOpts &opts, const std::optional<std::string> &loopMs)
{
	SamplingTiming t;
	if (opts.number.has_value()) {
		if (const auto n = parseInteger<int>(*opts.number); n && *n > 0) {
			t.iterations = *n;
		} else {
			ERR("Invalid value for --number: '{}'", opts.number->c_str());
			return std::nullopt;
		}
	}

	if (opts.interval.has_value()) {
		if (const auto sec = parseInteger<int>(*opts.interval);
			sec && *sec > 0 && std::chrono::seconds{*sec} <= MAX_INTERVAL) {
			t.interval = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds{*sec});
		} else {
			ERR("Invalid value for --interval: '{}'", opts.interval->c_str());
			return std::nullopt;
		}
	}

	if (loopMs.has_value()) {
		if (const auto ms = parseInteger<int>(*loopMs); ms && *ms > 0) {
			t.interval = std::chrono::milliseconds{*ms};
		} else {
			ERR("Invalid value for --loop-ms: '{}'", *loopMs);
			return std::nullopt;
		}
	}

	if (opts.time.has_value()) {
		if (opts.number.has_value()) {
			ERR("--time and --number cannot be used together.\n");
			return std::nullopt;
		}
		if (const auto parsed = parseInteger<int64_t>(*opts.time);
			parsed && *parsed >= 1 && *parsed <= MAX_DUMP_TIME_SECONDS) {
			t.totalTimeSeconds = *parsed;
		} else {
			ERR("Invalid value for --time: '{}'", opts.time->c_str());
			return std::nullopt;
		}
	}

	return t;
}

/**
 * @brief Aggregate return type for parseDumpCLI() on a successful parse.
 */
struct ParsedArgs
{
	DumpOpts opts;
	std::optional<std::string> loopMs;
};

/**
 * @brief Parse the @c dump / @c dmon command-line arguments using CLI11.
 *
 * Handles special cases before handing off to CLI11:
 *  - Bare @c "help" keyword (e.g. @c "xpu-smi dump help"): calls @p showHelp and returns 0.
 *  - @c --loop-ms (multi-character flag pre-extracted before CLI11 parsing): stored in
 *    ParsedArgs::loopMs.
 *
 * @param[in] cmdName   The canonical command name (@c "dump" or @c "dmon"), used as the
 *                      CLI11 app name for error messages.
 * @param[in] showHelp  Callable invoked to display help text on @c -h / @c --help or the
 *                      bare @c "help" keyword.
 * @param[in] args      Raw argument vector from the command dispatcher.
 * @retval std::variant<ParsedArgs, int>
 *         - ParsedArgs                           on a successful parse.
 *         - int (ZE_RESULT_SUCCESS)              when help was displayed.
 *         - int (ZE_RESULT_ERROR_INVALID_ARGUMENT) on a CLI11 parse error.
 */
std::variant<ParsedArgs, int> parseDumpCLI(std::string_view cmdName, const std::function<void()> &showHelp,
										   arg_struct *args)
{
	ParsedArgs parsed;

	CLI::App sub{std::string{cmdName}};
	sub.set_help_flag("-h,--help", "Display help");
	sub.add_flag("-j,--json", parsed.opts.json, "Output in JSON format");
	sub.add_option("-d,--device,--id", parsed.opts.device, "Device index or PCI BDF address (-1 = all)")
		->each([&](const std::string &val) {
			if (const auto n = parseInteger<int>(val); n && *n == -1) {
				parsed.opts.device.clear();
			}
		});
	sub.add_option("--metrics,--select", parsed.opts.metrics, "Metric query");
	sub.add_option("-f,--file,--filename", parsed.opts.file, "Output file path");
	sub.add_option("--time", parsed.opts.time, "Total dump duration in seconds");
	sub.add_flag("--date", parsed.opts.date, "Include date in timestamp");
	sub.add_option("--interval,--delay,--loop", parsed.opts.interval, "Sampling interval in seconds (default: 1)");
	sub.add_option("--number,--count", parsed.opts.number, "Number of samples");
	std::string formatStr;
	sub.add_option("--format", formatStr, "Output format: [csv][,noheader][,nounits]");

	// Skip command name (argv[1]) if it matches "dump" or "dmon" (our primary or alias name)
	int argStart = 1;
	if (args->argc > 1) {
		std::string_view const cmd{args->argv[1]};
		if (cmd == "dump" || cmd == "dmon") {
			argStart = 2;
		}
	}

	// Handle bare "help" keyword (e.g., "xpu-smi dump help" or "xpu-smi dmon help")
	if (args->argc == argStart + 1) {
		std::string_view const lastArg{args->argv[argStart]};
		if (lastArg == "help") {
			showHelp();
			return static_cast<int>(ZE_RESULT_SUCCESS);
		}
	}

	// Pre-extract and filter --loop-ms before CLI11 to avoid ambiguity with --loop
	auto [filteredArgs, loopMsValue] =
		filterArgvAndExtract(std::span{args->argv + 1, static_cast<std::size_t>(args->argc - 1)}, "", "--loop-ms");
	parsed.loopMs = loopMsValue;

	// Build argv pointers for CLI11 parsing (must use stable storage)
	std::vector<char *> filteredArgv;
	for (auto &arg : filteredArgs) {
		filteredArgv.push_back(arg.data());
	}

	try {
		sub.parse(static_cast<int>(filteredArgv.size()), filteredArgv.data());
	} catch (const CLI::CallForHelp &) {
		showHelp();
		return static_cast<int>(ZE_RESULT_SUCCESS);
	} catch (const CLI::ParseError &e) {
		ERR("{}\n", e.what());
		return static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT);
	}

	for (const auto tok : std::string_view{formatStr} | std::views::split(',')) {
		const std::string_view t{tok.begin(), tok.end()};
		if (t == "csv") {
			parsed.opts.csvFormat = true;
		} else if (t == "noheader") {
			parsed.opts.noheader = true;
		} else if (t == "nounits" || t == "nounit") {
			parsed.opts.nounits = true;
		}
	}

	return parsed;
}

/**
 * @brief Format the current wall-clock time as a CSV-ready timestamp string.
 *
 * The time point is floored to millisecond precision before formatting.
 *
 * @param[in] showDate  When @c true, prefix the time component with @c "YYYY/MM/DD ".
 * @return  Formatted string: @c "HH:MM:SS.mmm" or @c "YYYY/MM/DD HH:MM:SS.mmm".
 */
std::string getTimestamp(bool showDate)
{
	auto now = std::chrono::system_clock::now();
	auto nowMs = std::chrono::floor<std::chrono::milliseconds>(now);
	return showDate ? std::format("{:%Y/%m/%d %H:%M:%S}", nowMs) : std::format("{:%H:%M:%S}", nowMs);
}

/**
 * @brief MetricOutput sink that formats and writes one CSV line per device per sample.
 *
 * Implements the MetricOutput concept expected by metrics::runMetricsWithCaches():
 * @c onBegin, @c onBeginDevice, @c onMetric, @c onEndDevice, @c onEnd.
 *
 * Each row is flushed to disk immediately after @c onEndDevice() to support live file
 * tailing. When @c useFile is @c false, output is sent to stdout via @c PRINT.
 *
 * @note  Set @c prependTimestamp = @c false and @c prependDeviceId = @c false when
 *        those columns are provided as explicit metric fields (e.g. @c --query-gpu).
 */
struct DumpOutput
{
	bool showDate;
	bool useFile;
	std::ofstream *dumpFile;
	bool json{false};			 // emit JSON Lines instead of CSV
	bool prependTimestamp{true}; // false for --query-gpu (timestamp is an explicit field)
	bool prependDeviceId{true};	 // false for --query-gpu (index is an explicit field)
	bool aligned{false};		 // true when stdout is a TTY: use space-padded columns instead of CSV
	bool noheader{false};		 // suppress header row entirely
	bool nounits{false};		 // strip unit suffixes from column headers

	DumpOutput(bool date, bool toFile, std::ofstream *file) : showDate{date}, useFile{toFile}, dumpFile{file} {}

	void onBegin(std::span<const metrics::QueryMetric *> fields)
	{
		fieldDefs.assign(fields.begin(), fields.end());

		// Build the column formatter for use in onEndDevice() regardless of header suppression.
		if (!json) {
			alignedFormatter.emplace();
			if (prependTimestamp) {
				alignedFormatter->addColumn("Timestamp", showDate ? 23 : 12, Align::Left);
			}
			if (prependDeviceId) {
				alignedFormatter->addColumn("DeviceId", 8, Align::Right);
			}
			for (const auto *f : fieldDefs) {
				const std::string label =
					(!nounits && !f->unit.empty()) ? std::format("{} ({})", f->name, f->unit) : std::string{f->name};
				alignedFormatter->addColumn(label, std::max(static_cast<int>(label.size()), 6), Align::Right);
			}
			alignedFormatter->lockWidths();
		}

		// Emit header unless suppressed (file headers written by run(); JSON has none).
		if (json || noheader || headerEmitted || useFile) {
			headerEmitted = true;
			return;
		}
		headerEmitted = true;
		emit(aligned ? alignedFormatter->headerLine() : alignedFormatter->headerLine(TableBuilder::LineStyle::Csv));
	}

	void onBeginDevice(devInfo &dev)
	{
		currentDev = &dev;
		row.clear();
	}

	void onMetric([[maybe_unused]] const metrics::QueryMetric &f, const std::string &val) { row.push_back(val); }

	void onEndDevice([[maybe_unused]] devInfo & /*unused*/)
	{
		if (json) {
			nlohmann::ordered_json obj;
			if (prependTimestamp) {
				obj["timestamp"] = getTimestamp(showDate);
			}
			if (prependDeviceId) {
				obj["device"] = currentDev->index;
			}

			const bool hasEnvelope = prependTimestamp || prependDeviceId;
			nlohmann::ordered_json metricsHolder;
			nlohmann::ordered_json &metricsTarget = hasEnvelope ? metricsHolder : obj;

			for (auto i : std::views::iota(std::size_t{0}, std::min(row.size(), fieldDefs.size()))) {
				metricsTarget[std::string{fieldDefs[i]->name}] = row[i];
			}
			if (hasEnvelope) {
				obj["metrics"] = std::move(metricsHolder);
			}
			emit(obj.dump());
			return;
		}

		std::vector<std::string> rowValues;
		if (prependTimestamp) {
			rowValues.push_back(getTimestamp(showDate));
		}
		if (prependDeviceId) {
			rowValues.push_back(std::to_string(currentDev->index));
		}
		for (const auto &v : row) {
			rowValues.push_back(v);
		}
		emit(aligned ? alignedFormatter->rowLine(rowValues)
					 : alignedFormatter->rowLine(rowValues, TableBuilder::LineStyle::Csv));
	}

	void onEnd() {}

private:
	void emit(const std::string &line) const
	{
		if (useFile && (dumpFile != nullptr)) {
			*dumpFile << line << "\n";
			dumpFile->flush();
		} else {
			PRINT("{}", line);
		}
	}

	std::vector<std::string> row;
	std::vector<const metrics::QueryMetric *> fieldDefs;
	std::optional<TableBuilder> alignedFormatter;
	bool headerEmitted{false};
	devInfo *currentDev{nullptr};
};

/**
 * @brief Continuously re-sample metrics in loop mode for @c --query-gpu.
 *
 * The first sample has already been emitted by cmdDump::runQuery() before this function
 * is called.  This function emits the second and subsequent samples at @p loopInterval
 * intervals until @p count samples have been emitted in total, or the user presses
 * @c q / @c Q / ESC / Ctrl-C.
 *
 * @pre   @p fields must be non-empty.
 * @pre   @p deviceList and @p caches must have equal size and be pre-populated by the caller.
 * @pre   @p count must be 0 (infinite) or a positive integer.
 *
 * @param[in]     out           Metric output sink (taken by value; owned by this function).
 * @param[in]     fields        Span of resolved QueryMetric descriptors to sample.
 * @param[in,out] deviceList    Device handles; mutated by populateMetricCacheContinuous().
 * @param[in]     caches        Initial metric caches from the first sample (moved in).
 * @param[in]     loopInterval  Delay between consecutive sample collections.
 * @param[in]     count         Total number of samples to emit (0 = unlimited).
 * @retval ZE_RESULT_SUCCESS  Always; per-metric errors are logged by the metric layer.
 *
 * @note  The keyboard-input thread uses the @c std::jthread stop token for cooperative
 *        cancellation; the thread exits after the next key press once a stop is requested.
 */
ze_result_t runQueryLoopMode(DumpOutput out, std::span<const metrics::QueryMetric *> fields,
							 std::vector<devInfo> &deviceList, std::vector<metrics::MetricCache> caches,
							 std::chrono::milliseconds loopInterval, int count)
{
	int remaining = count; // 0 = infinite
	if (remaining == 1) {
		return ZE_RESULT_SUCCESS; // already emitted the only sample
	}
	if (remaining > 1) {
		--remaining; // first sample was already emitted
	}

	std::jthread inputThread([&](const std::stop_token &stoken) {
		char ch = 0;
		while (!stoken.stop_requested()) {
			ch = GETCH();
			if (ch == 'q' || ch == 'Q' || ch == 27 || ch == 3) {
				return;
			}
		}
	});

	const std::size_t numDevices = deviceList.size();
	auto stoken = inputThread.get_stop_token();

	while (!stoken.stop_requested()) {
		std::this_thread::sleep_for(loopInterval);

		for (std::size_t i = 0; i < numDevices; ++i) {
			caches[i] = metrics::populateMetricCacheContinuous(deviceList[i], caches[i]);
		}

		metrics::runMetricsWithCaches(out, fields, std::span<devInfo>(deviceList),
									  std::span<const metrics::MetricCache>(caches));

		if (remaining > 0 && --remaining == 0) {
			inputThread.request_stop();
			break;
		}
	}

	// std::jthread automatically joins on destruction - no need to manually join/detach
	RESTORE_TERMINAL();
	return ZE_RESULT_SUCCESS;
}

/**
 * @brief Main continuous-sampling loop for the @c dump command.
 *
 * Initialises metric caches, emits the first sample after one @p timing.interval window,
 * then loops until a stop condition is met:
 *  - @p timing.iterations samples have been emitted, or
 *  - @p timing.totalTimeSeconds wall-clock seconds have elapsed, or
 *  - The user presses @c q / @c Q / ESC / Ctrl-C (interactive TTY only).
 *
 * A keyboard-input thread is spawned only when appropriate: stdin is a TTY, no fixed
 * sample count was requested, and no fixed total duration was set.
 *
 * @pre   @p fields must be non-empty.
 * @pre   If @p useFile is @c true, @p dumpFile must already be open for writing.
 *
 * @param[in]     out        Metric output sink (taken by value; owned by this function).
 * @param[in]     fields     Span of resolved QueryMetric descriptors to sample.
 * @param[in,out] deviceList Device handles; mutated each iteration by the metric layer.
 * @param[in]     timing     Resolved sampling-timing parameters (interval, count, duration).
 * @param[in]     useFile    @c true when output is directed to @p dumpFile instead of stdout.
 * @param[in,out] dumpFile   Output file stream; closed on function exit when @p useFile is @c true.
 * @retval ZE_RESULT_SUCCESS  Always; per-metric errors are logged by the metric layer.
 *
 * @note  A keyboard-input thread is spawned only for interactive TTY sessions without a
 *        fixed sample count or duration. The thread is detached rather than joined because
 *        GETCH() blocks indefinitely; cooperative cancellation uses @c std::stop_source /
 *        @c std::stop_token so the main loop and input thread share a single quit signal.
 */
ze_result_t runOutputLoop(DumpOutput out, std::span<const metrics::QueryMetric *> fields,
						  std::vector<devInfo> &deviceList, const SamplingTiming &timing, bool useFile,
						  std::ofstream &dumpFile)
{
	// Shared quit signal: either the user presses q/ESC/Ctrl-C or the main loop
	// exhausts its iteration/time budget. Both sides write to the same stop_source.
	std::stop_source quitSource;
	auto quitToken = quitSource.get_token();

	int iter = timing.iterations;
	const bool shouldStartInputThread =
		STDIN_ISATTY() && ((iter < 0 && !useFile) || (useFile && timing.totalTimeSeconds < 0));

	std::jthread inputThread;
	if (shouldStartInputThread) {
		inputThread = std::jthread([quitSource, quitToken](const std::stop_token &ownStop) mutable {
			char ch = 0;
			while (!ownStop.stop_requested() && !quitToken.stop_requested()) {
				ch = GETCH();
				if (ch == 'q' || ch == 'Q' || ch == 27 || ch == 3) {
					quitSource.request_stop();
					return;
				}
			}
		});
	}

	const auto startTime = std::chrono::steady_clock::now();
	const std::size_t numDevices = deviceList.size();
	std::vector<metrics::MetricCache> caches(numDevices);

	const auto firstSampleDeadline = startTime + timing.interval;
	for (std::size_t i = 0; i < numDevices; ++i) {
		caches[i] = metrics::populateMetricCacheBegin(deviceList[i]);
	}
	std::this_thread::sleep_until(firstSampleDeadline);
	for (std::size_t i = 0; i < numDevices; ++i) {
		metrics::populateMetricCacheEnd(deviceList[i], caches[i]);
	}

	metrics::runMetricsWithCaches(out, std::span<const metrics::QueryMetric *>{fields}, std::span<devInfo>(deviceList),
								  std::span<const metrics::MetricCache>{caches});
	if (iter > 0 && --iter == 0) {
		quitSource.request_stop();
	}

	if (!quitToken.stop_requested() && iter != 0) {
		std::this_thread::sleep_for(timing.interval);
	}

	while (!quitToken.stop_requested()) {
		const auto cycleStartTime = std::chrono::steady_clock::now();

		for (std::size_t i = 0; i < numDevices; ++i) {
			caches[i] = metrics::populateMetricCacheContinuous(deviceList[i], caches[i]);
		}
		metrics::runMetricsWithCaches(out, std::span<const metrics::QueryMetric *>{fields},
									  std::span<devInfo>(deviceList), std::span<const metrics::MetricCache>{caches});

		const auto collectionTimeMs =
			std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - cycleStartTime);
		const auto remainingSleep = timing.interval - collectionTimeMs;
		if (remainingSleep.count() > 0) {
			std::this_thread::sleep_for(remainingSleep);
		}

		if (iter > 0 && --iter == 0) {
			quitSource.request_stop();
			break;
		}

		if (timing.totalTimeSeconds > 0) {
			const auto elapsedMs =
				std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime)
					.count();
			if (elapsedMs >= timing.totalTimeSeconds * 1000LL) {
				quitSource.request_stop();
				break;
			}
		}
	}

	if (useFile) {
		dumpFile.close();
		PRINT("\nDumping is stopped.\n");
	}

	if (inputThread.joinable()) {
		inputThread.detach();
	}
	RESTORE_TERMINAL();
	return ZE_RESULT_SUCCESS;
}

} // namespace

// -- runQuery ---------------------------------------------------------

int cmdDump::runQuery(const std::string &metrics, const std::string &deviceSpec, arg_struct *args, QueryFormat fmt)
{
	// --query-gpu accepts dot-notation field names and group aliases, not legacy numeric IDs.
	for (auto &&rng : std::string_view{metrics} | std::views::split(',')) {
		std::string_view sv{rng.begin(), rng.end()};
		const auto b = sv.find_first_not_of(" \t");
		if (b == std::string_view::npos) {
			continue;
		}
		if (parseInteger<int>(sv.substr(b, sv.find_last_not_of(" \t") - b + 1)).has_value()) {
			ERR("Numeric metric IDs are not supported by --query-gpu.\n");
			ERR("Use field names like 'temperature.gpu,power.draw' or group aliases like 'pu'.\n");
			return ZE_RESULT_ERROR_INVALID_ARGUMENT;
		}
	}

	auto fields = metrics::resolveQuery(metrics);
	if (fields.empty()) {
		ERR("No valid metrics matched: '{}'", metrics.c_str());
		ERR("Use field names like 'temperature.gpu,power.draw' or aliases like 'pu' (POWER+UTILIZATION).\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	std::vector<devInfo> deviceList;
	ze_result_t const result = args->sm.findDevice(deviceSpec.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '{}'\n", deviceSpec.c_str());
		return result;
	}

	DumpOutput out{false, false, nullptr};
	out.json = fmt.json;
	out.noheader = fmt.noheader;
	out.nounits = fmt.nounits;
	out.prependTimestamp = false;
	out.prependDeviceId = false;

	const std::size_t numDevices = deviceList.size();
	std::vector<metrics::MetricCache> caches(numDevices);
	const auto sampleDeadline = std::chrono::steady_clock::now() + metrics::detail::SAMPLE_WINDOW;
	for (std::size_t i = 0; i < numDevices; ++i) {
		caches[i] = metrics::populateMetricCacheBegin(deviceList[i]);
	}
	std::this_thread::sleep_until(sampleDeadline);
	for (std::size_t i = 0; i < numDevices; ++i) {
		metrics::populateMetricCacheEnd(deviceList[i], caches[i]);
	}
	metrics::runMetricsWithCaches(out, std::span<const metrics::QueryMetric *>(fields), std::span<devInfo>(deviceList),
								  std::span<const metrics::MetricCache>(caches));

	if (fmt.loopMs > 0) {
		return runQueryLoopMode(std::move(out), fields, deviceList, std::move(caches),
								std::chrono::milliseconds{fmt.loopMs}, fmt.count);
	}

	return ZE_RESULT_SUCCESS;
}

// -- printQueryHelp -----------------------------------------------------------

void cmdDump::printQueryHelp()
{
	std::vector<helpCmd> helpList;

	helpList.emplace_back(TITLE, "Query individual per-GPU metric fields");
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Usage: %s --query-gpu=<fields> [Options]", progName.c_str());
	helpList.emplace_back(HEADING, "%s --query-gpu=temperature.gpu,power.draw --id=0 --loop=5", progName.c_str());
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Options:");
	helpList.emplace_back(HEADING, "--id=<n>                    Device index or PCI BDF (default: all)");
	helpList.emplace_back(HEADING, "--loop[=<sec>]              Repeat every N seconds (default: 1)");
	helpList.emplace_back(HEADING, "--loop-ms=<ms>              Repeat every N milliseconds");
	helpList.emplace_back(HEADING, "--count=<n>                 Number of iterations (default: infinite)");
	helpList.emplace_back(HEADING, "--format=csv[,noheader][,nounits]  Output format");
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Available fields: (legacy ID in brackets)");
	for (const metrics::QueryMetric &f : metrics::getQueryMetrics()) {
		std::optional<int> legacyId;
		for (const auto &[id, metricName] : LEGACY_METRIC_NAMES) {
			if (metricName == f.name && (!legacyId.has_value() || id < *legacyId)) {
				legacyId = id;
			}
		}
		std::string fieldName = std::string(f.name);
		if (legacyId.has_value()) {
			fieldName += std::format(" [{}]", *legacyId);
		}
		if (f.unit.empty()) {
			helpList.emplace_back(SUB_HEADING, "%-40s %s", fieldName.c_str(), f.description.data());
		} else {
			helpList.emplace_back(SUB_HEADING, "%-40s %s (%s)", fieldName.c_str(), f.description.data(), f.unit.data());
		}
	}
	helpList.emplace_back(BLANK);

	for (const auto &entry : helpList) {
		PRINT("{0:<{1}}{2}\n", "", entry.char_gap, entry.line);
	}
}

// -- listGpus ─────────────────────────────────────────────────────────────────

int cmdDump::listGpus(arg_struct *args)
{
	TRACING();
	std::vector<devInfo> deviceList;
	const ze_result_t result = args->sm.findDevice("", &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Failed to enumerate GPU devices\n");
		return result;
	}
	metrics::MetricCache const dummy{};
	const auto nameMetric = metrics::findMetric("name");
	const auto uuidMetric = metrics::findMetric("uuid");
	const auto indexMetric = metrics::findMetric("index");
	for (auto &dev : deviceList) {
		std::string gpuName = "N/A";
		std::string gpuUuid = "N/A";
		std::string gpuIdx = "N/A";
		if (nameMetric) {
			nameMetric->getter(dev, gpuName, dummy);
		}
		if (uuidMetric) {
			uuidMetric->getter(dev, gpuUuid, dummy);
		}
		if (indexMetric) {
			indexMetric->getter(dev, gpuIdx, dummy);
		}
		PRINT("GPU {}: {} (UUID: GPU-{})\n", gpuIdx.c_str(), gpuName.c_str(), gpuUuid.c_str());
	}
	return ZE_RESULT_SUCCESS;
}

// -- help
// ---------------------------------------------------------

void cmdDump::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.emplace_back(TITLE, "Dump device statistics data");
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Usage: %s dump [Options]", progName.c_str());
	helpList.emplace_back(HEADING, "%s dump --device [id] --metrics [metrics] --interval [secs] --number [count]",
						  progName.c_str());
	helpList.emplace_back(HEADING,
						  "%s dump --device [id] --metrics [metrics] -f [filename] --loop-ms [ms] --time [secs]",
						  progName.c_str());
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Options:");
	helpList.emplace_back(HEADING, "-h,--help                   Print this help message and exit");
	helpList.emplace_back(HEADING, "-j,--json                   Print result in JSON format");
	helpList.emplace_back(BLANK);
	helpList.emplace_back(HEADING, "--device,--id               Device index or PCI BDF address (-1 = all)");
	helpList.emplace_back(HEADING, "--metrics,--select          Comma-separated group names or aliases:");
	for (const auto &line : buildGroupAliasLines()) {
		helpList.emplace_back(SUB_HEADING, "%s", line.c_str());
	}
	helpList.emplace_back(SUB_HEADING, "Multi-char aliases: e.g. \"pu\" = POWER+TEMPERATURE+UTILIZATION");
	helpList.emplace_back(SUB_HEADING, "For individual field names use --query-gpu instead.");
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Available fields: (legacy ID in brackets)");
	for (const metrics::QueryMetric &f : metrics::getQueryMetrics()) {
		// Find the lowest legacy numeric ID for this field, if any
		std::optional<int> legacyId;
		for (const auto &[id, metricName] : LEGACY_METRIC_NAMES) {
			if (metricName == f.name && (!legacyId.has_value() || id < *legacyId)) {
				legacyId = id;
			}
		}

		std::string fieldName = std::string(f.name);
		if (legacyId.has_value()) {
			fieldName += std::format(" [{}]", *legacyId);
		}

		if (f.unit.empty()) {
			helpList.emplace_back(SUB_HEADING, "%-40s %s", fieldName.c_str(), f.description.data());
		} else {
			helpList.emplace_back(SUB_HEADING, "%-40s %s (%s)", fieldName.c_str(), f.description.data(), f.unit.data());
		}
	}
	helpList.emplace_back(BLANK);
	helpList.emplace_back(HEADING, "--interval,--delay          Sampling interval in seconds (default: 1)");
	helpList.emplace_back(HEADING, "--number,--count            Number of samples (default: unlimited)");
	helpList.emplace_back(HEADING,
						  "--loop-ms                   Sampling interval in milliseconds (overrides --interval)");
	helpList.emplace_back(BLANK);
	helpList.emplace_back(HEADING, "-f,--file,--filename        Write output to a file instead of stdout");
	helpList.emplace_back(HEADING, "--time                      Total dump duration in seconds");
	helpList.emplace_back(HEADING, "--date                      Prefix timestamps with date");
	helpList.emplace_back(HEADING, "--format=csv[,noheader][,nounits]  Force CSV output; suppress header or units");
	helpList.emplace_back(BLANK);

	printHelp(helpList, helpType);
}

// -- run
// -----------------------------------------------------------

int cmdDump::run(arg_struct *args)
{
	TRACING();

	if (args->argc == 2) {
		help();
		return ZE_RESULT_SUCCESS;
	}

	auto parseResult = parseDumpCLI(name, [this] { help(); }, args);
	if (const int *code = std::get_if<int>(&parseResult)) {
		return *code;
	}
	auto &[opts, loopMs] = std::get<ParsedArgs>(parseResult);

	if (opts.file.has_value() && !opts.metrics.has_value()) {
		ERR("--file requires --metrics\n");
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (!opts.metrics.has_value()) {
		help();
		return ZE_RESULT_SUCCESS;
	}

	// dump -m accepts group names, aliases, and legacy numeric IDs only.
	// Dot-notation field names (e.g. "temperature.gpu") belong to --query-gpu.
	auto fields = resolveMetricsArg(*opts.metrics);
	if (fields.empty()) {
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	const bool hasPowerMetrics = std::ranges::any_of(
		fields, [](const metrics::QueryMetric *f) { return f->name == "power.draw" || f->name == "power.draw.gpu"; });

	const auto timing = parseSamplingTiming(opts, loopMs);
	if (!timing) {
		return ZE_RESULT_ERROR_INVALID_ARGUMENT;
	}

	if (hasPowerMetrics && timing->interval.count() < 50) {
		PRINT("Warning: Power metrics are unreliable with sample windows < 50ms.\n");
		PRINT("Recommend using at least 50ms (--loop-ms=50) for stable power.draw readings.\n");
	}

	std::vector<devInfo> deviceList;
	ze_result_t const result = args->sm.findDevice(opts.device.c_str(), &deviceList);
	if (result != ZE_RESULT_SUCCESS) {
		ERR("Error: Device handle not found for device ID '{}'", opts.device.c_str());
		return result;
	}

	std::ofstream dumpFile;
	const bool useFile = opts.file.has_value();
	if (useFile) {
		dumpFile.open(*opts.file, std::ios::out | std::ios::trunc);
		if (!dumpFile.is_open()) {
			ERR("Failed to open file '{}'", opts.file->c_str());
			return ZE_RESULT_ERROR_UNKNOWN;
		}
		if (!opts.json && !opts.noheader) {
			std::string header = "Timestamp, DeviceId";
			for (const metrics::QueryMetric *const f : fields) {
				header += (!opts.nounits && !f->unit.empty()) ? std::format(", {} ({})", f->name, f->unit)
															  : std::format(", {}", f->name);
			}
			dumpFile << header << "\n";
		}
		if (!opts.time.has_value() && STDIN_ISATTY()) {
			PRINT("Dump data to file {}. Press q or ESC to stop.\n", opts.file->c_str());
		} else {
			PRINT("Dump data to file {}.\n", opts.file->c_str());
		}
	} else if (!opts.json) {
		// TTY: aligned header emitted by DumpOutput::onBegin().
	}

	DumpOutput out{opts.date, useFile, useFile ? &dumpFile : nullptr};
	out.json = opts.json;
	out.noheader = opts.noheader;
	out.nounits = opts.nounits;
	out.aligned = !useFile && !opts.json && !opts.csvFormat && STDIN_ISATTY();
	return runOutputLoop(out, fields, deviceList, *timing, useFile, dumpFile);
}
