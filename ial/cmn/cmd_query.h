/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#ifndef _CMD_QUERY_H
#define _CMD_QUERY_H

#include "cmd_ps.h"
#include "cmds.h"
#include "metrics_registry.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace query {

/// Column (0-based) at which the ": " separator starts on every leaf line.
inline constexpr std::size_t KEY_COLUMN = 42;
inline constexpr std::size_t INDENT_WIDTH = 4;
inline constexpr std::string_view NOT_AVAILABLE = "N/A";
inline constexpr std::string_view LOG_BANNER = "==============XPU-SMI LOG==============";

/**
 * One node of the report tree.
 *
 * A node with no children and not a list is a leaf: its value, or N/A when absent, is
 * printed after the label. A node with children and no value is a section title. A node
 * with both a value and children is a record, such as one process: it prints as a leaf
 * and its children follow one level deeper.
 */
struct QueryNode
{
	std::string label;				  ///< Text label, e.g. "Power Draw".
	std::string key;				  ///< JSON key, e.g. "power_draw".
	std::optional<std::string> value; ///< Leaf or record value; nullopt prints as N/A and serializes as null.
	std::string_view unit;			  ///< Appended after the value in text output only; empty for none.
	std::vector<QueryNode> children;
	bool isList = false; ///< Children are repeated records; an empty list prints "None".
	/// Left out of the text report when it has no value; JSON still has the key, as null. For rows
	/// that only some devices or drivers supply, which would otherwise read N/A everywhere else.
	bool hideWhenAbsent = false;

	[[nodiscard]] bool isLeaf() const noexcept { return children.empty() && !isList; }

	/// Whether the text report prints the node: not a hidden row without a value, nor a section
	/// all of whose rows are left out.
	[[nodiscard]] bool shownInText() const noexcept;
};

/// Sections printed without --display: everything except the supported clocks, which list every
/// clock the GPU can be set to and are shown only when SUPPORTED_CLOCKS or ALL is named.
inline constexpr metrics::MetricGroup DEFAULT_SECTIONS =
	static_cast<metrics::MetricGroup>(metrics::detail::toUnderlying(metrics::MetricGroup::ALL) &
									  ~metrics::detail::toUnderlying(metrics::MetricGroup::SUPPORTED_CLOCKS));

/// What the user selected with --display. Default-constructed selects the default sections.
struct QuerySelection
{
	metrics::MetricGroup groups = DEFAULT_SECTIONS;
	bool processes = true;
};

/// A static property of one device, gathered once per run.
struct StaticValue
{
	std::string_view key;
	std::optional<std::string> value; ///< nullopt when the property could not be read.
};

/// Static properties of one device. A key that is absent does not apply to the device.
struct QueryStaticProps
{
	std::vector<StaticValue> values;

	[[nodiscard]] bool has(std::string_view key) const;
	[[nodiscard]] std::optional<std::string> get(std::string_view key) const;
};

/// Registry getter output by field name; nullopt when the getter failed.
using MetricValues = std::unordered_map<std::string_view, std::optional<std::string>>;

/// Everything collected for one invocation.
struct QueryReport
{
	std::string timestamp;
	std::optional<std::string> umdVersion;
	std::optional<std::string> kmdVersion;
	std::optional<std::string> levelZeroVersion;
	/// One node per device in index order. The node's value is the PCI address; its label is
	/// the text header line; its children are the device's sections.
	std::vector<QueryNode> gpus;
};

/// PCI address split into its fields.
struct BdfParts
{
	uint32_t domain = 0;
	uint32_t bus = 0;
	uint32_t device = 0;
	uint32_t function = 0;
};

/// Section names accepted by --display: metrics::sectionNames() plus "PIDS".
[[nodiscard]] std::vector<std::string_view> sectionNames();

/**
 * @brief Parse a comma-separated --display value.
 *
 * @param[in] csv Section names, case-insensitive, surrounding blanks ignored.
 *
 * @retval selection Every token named a section.
 * @retval nullopt   The value is empty or any token is unknown.
 */
[[nodiscard]] std::optional<QuerySelection> parseSelection(std::string_view csv);

/// Split "dddd:bb:dd.f" into its fields; nullopt when @p bdf is not that shape.
[[nodiscard]] std::optional<BdfParts> bdfParts(std::string_view bdf) noexcept;

/// Registry fields the report reads, in report order, without duplicates.
[[nodiscard]] std::span<const std::string_view> referencedMetricNames();

/// The referenced registry fields that @p selection shows.
[[nodiscard]] std::vector<const metrics::QueryMetric *> selectedMetrics(const QuerySelection &selection);

/// Static property keys the report reads, with the section that shows each.
struct StaticKey
{
	std::string_view key;
	metrics::MetricGroup group;
	bool amcOnly;
};
[[nodiscard]] std::span<const StaticKey> staticKeys();

/// Render one node and its descendants as text lines appended to @p out.
void renderNode(const QueryNode &node, std::size_t depth, std::string &out);
[[nodiscard]] std::string renderText(const QueryReport &report);
[[nodiscard]] nlohmann::ordered_json nodeToJson(const QueryNode &node);
[[nodiscard]] nlohmann::ordered_json toJson(const QueryReport &report);

[[nodiscard]] QueryNode processNode(const psInfo &proc);
[[nodiscard]] QueryNode processesSection(std::span<const psInfo> procs);

/**
 * @brief Build one device's report tree from values that were already collected.
 *
 * Touches no hardware. A leaf is shown when the section that owns its field is selected,
 * and a section with no shown leaf is left out.
 *
 * @param[in] bdf          PCI address of the device.
 * @param[in] selection    Sections to show.
 * @param[in] metricValues Registry getter output by field name.
 * @param[in] staticProps      Static properties of the device.
 * @param[in] procs        Processes using the device.
 *
 * @return The device node: the PCI address as its value and the sections as children.
 */
[[nodiscard]] QueryNode buildDeviceNode(std::string_view bdf, const QuerySelection &selection,
										const MetricValues &metricValues, const QueryStaticProps &staticProps,
										std::span<const psInfo> procs);

/// A device in survivability mode: only its state and the way to recover it.
[[nodiscard]] QueryNode survivabilityNode(std::string_view bdf);

} // namespace query

/** Parsed command line of the query command. */
struct QueryOptions
{
	query::QuerySelection selection{};
	std::string deviceSpec; ///< Empty selects every device.
	bool json = false;
	std::chrono::milliseconds loop{0}; ///< Time between frames; 0 renders one frame.
	int count = 0;					   ///< Frame limit; 0 is unlimited.
	std::optional<std::string> file;
};

class cmdQuery : public cmds
{
public:
	cmdQuery() { name = "query"; }
	~cmdQuery() override = default;
	cmdQuery(const cmdQuery &) = delete;
	cmdQuery &operator=(const cmdQuery &) = delete;
	cmdQuery(cmdQuery &&) = delete;
	cmdQuery &operator=(cmdQuery &&) = delete;

	void help(HELP helpType = FULL_HELP) override;
	int run(arg_struct *args) override;

	/**
	 * @brief Parse the arguments after the subcommand name.
	 *
	 * @param[in] args Command line as the dispatcher received it.
	 *
	 * @retval QueryOptions The options to run with.
	 * @retval int          The exit code to return: help was printed or the arguments were rejected.
	 */
	[[nodiscard]] static std::variant<QueryOptions, int> parseOptions(arg_struct *args);

protected:
	[[nodiscard]] DriverMode driverMode(std::span<char *const> cmdArgs) const override;
};

#endif // _CMD_QUERY_H
