/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

/**
 * @file query_test.cpp
 * @brief Unit tests for the query command: section selection, text and JSON rendering, the
 *        report layout, option parsing and a full build over a device that answers nothing.
 *        No GPU is needed.
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#ifdef INFO
#undef INFO
#endif

#include <string>
std::string progName = "test";

#include "cmd_query.h"
#include "metrics/clock.h"
#include "metrics_registry.h"
#include <algorithm>
#include <ranges>
#include <sstream>
#include <string_view>
#include <vector>

#ifdef INFO
#undef INFO
#endif

using namespace query;

namespace {

/// Builds a stable arg_struct from a list of C strings, as the dispatcher would see them.
struct FakeArgs
{
	std::vector<std::string> storage;
	std::vector<char *> ptrs;
	arg_struct args{};

	explicit FakeArgs(std::initializer_list<const char *> argv)
	{
		for (const char *s : argv) {
			storage.emplace_back(s);
		}
		for (auto &s : storage) {
			ptrs.push_back(s.data());
		}
		args.argc = static_cast<int>(ptrs.size());
		args.argv = ptrs.data();
	}
};

[[nodiscard]] std::vector<std::string> linesOf(const std::string &text)
{
	std::vector<std::string> lines;
	std::istringstream in{text};
	for (std::string line; std::getline(in, line);) {
		lines.push_back(line);
	}
	return lines;
}

[[nodiscard]] std::size_t countOf(const std::string &haystack, std::string_view needle)
{
	std::size_t found = 0;
	for (auto pos = haystack.find(needle); pos != std::string::npos; pos = haystack.find(needle, pos + 1)) {
		++found;
	}
	return found;
}

[[nodiscard]] QueryNode leaf(std::string label, std::optional<std::string> value, std::string_view unit = {})
{
	QueryNode node;
	node.key = label;
	node.label = std::move(label);
	node.value = std::move(value);
	node.unit = unit;
	return node;
}

[[nodiscard]] QueryNode section(std::string label, std::vector<QueryNode> children)
{
	QueryNode node;
	node.key = label;
	node.label = std::move(label);
	node.children = std::move(children);
	return node;
}

[[nodiscard]] std::string rendered(const QueryNode &node, std::size_t depth = 0)
{
	std::string out;
	renderNode(node, depth, out);
	return out;
}

/// Every referenced registry field reads "M:<name>" and every static property "S:<key>".
struct MarkedInputs
{
	MetricValues metricValues;
	QueryStatics statics;
	std::vector<std::string> storage;

	MarkedInputs()
	{
		storage.reserve(referencedMetricNames().size() + staticKeys().size());
		for (const auto name : referencedMetricNames()) {
			metricValues[name] = "M:" + std::string{name};
		}
		for (const auto &known : staticKeys()) {
			// The allocation size is converted from bytes, so it needs a number to be shown.
			statics.values.push_back(
				{known.key, known.key == "max_mem_alloc_size" ? "1048576" : "S:" + std::string{known.key}});
		}
	}
};

[[nodiscard]] std::vector<std::string> topLevelLabels(const QueryNode &device)
{
	std::vector<std::string> labels;
	for (const auto &child : device.children) {
		labels.push_back(child.label);
	}
	return labels;
}

} // namespace

// ---------------------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------------------

TEST_SUITE("query selection")
{
	TEST_CASE("one section selects only that section and no processes")
	{
		const auto selection = parseSelection("MEMORY");
		REQUIRE(selection.has_value());
		CHECK(selection->groups == metrics::MetricGroup::MEMORY);
		CHECK_FALSE(selection->processes);
	}

	TEST_CASE("names are case-insensitive and blanks around them are ignored")
	{
		const auto selection = parseSelection("memory, Ecc");
		REQUIRE(selection.has_value());
		CHECK(selection->groups == (metrics::MetricGroup::MEMORY | metrics::MetricGroup::ECC));
	}

	TEST_CASE("PIDS selects the process list alone")
	{
		const auto selection = parseSelection("PIDS");
		REQUIRE(selection.has_value());
		CHECK(selection->groups == metrics::MetricGroup::NONE);
		CHECK(selection->processes);
	}

	TEST_CASE("ALL selects every section and the process list")
	{
		const auto selection = parseSelection("ALL");
		REQUIRE(selection.has_value());
		CHECK(selection->groups == metrics::MetricGroup::ALL);
		CHECK(selection->processes);
	}

	TEST_CASE("an empty value or an unknown name is rejected")
	{
		CHECK_FALSE(parseSelection("").has_value());
		CHECK_FALSE(parseSelection(" , ").has_value());
		CHECK_FALSE(parseSelection("MEMORY,BOGUS").has_value());
		CHECK_FALSE(parseSelection("m").has_value());
		CHECK_FALSE(parseSelection("pu").has_value());
	}

	TEST_CASE("every section the registry advertises is accepted")
	{
		for (const auto name : metrics::sectionNames()) {
			CHECK_MESSAGE(parseSelection(name).has_value(), name);
		}
	}

	TEST_CASE("the accepted names are the registry sections plus PIDS")
	{
		const auto names = sectionNames();
		const auto registry = metrics::sectionNames();
		REQUIRE(names.size() == registry.size() + 1);
		CHECK(std::ranges::equal(registry, std::span{names}.first(registry.size())));
		CHECK(names.back() == "PIDS");
	}
}

// ---------------------------------------------------------------------------------------
// PCI address
// ---------------------------------------------------------------------------------------

TEST_SUITE("query bdfParts")
{
	TEST_CASE("a PCI address splits into its four fields")
	{
		const auto parts = bdfParts("0000:03:00.0");
		REQUIRE(parts.has_value());
		CHECK(parts->domain == 0x0000);
		CHECK(parts->bus == 0x03);
		CHECK(parts->device == 0x00);
		CHECK(parts->function == 0x0);
	}

	TEST_CASE("hex digits of any case are read")
	{
		const auto parts = bdfParts("00aF:Bc:1d.7");
		REQUIRE(parts.has_value());
		CHECK(parts->domain == 0x00af);
		CHECK(parts->bus == 0xbc);
		CHECK(parts->device == 0x1d);
		CHECK(parts->function == 0x7);
	}

	TEST_CASE("anything that is not that shape is rejected")
	{
		CHECK_FALSE(bdfParts("").has_value());
		CHECK_FALSE(bdfParts("junk").has_value());
		CHECK_FALSE(bdfParts("0000:03:00:0").has_value());
		CHECK_FALSE(bdfParts("0000:0g:00.0").has_value());
		CHECK_FALSE(bdfParts("0000:03:00.00").has_value());
	}
}

// ---------------------------------------------------------------------------------------
// Text rendering
// ---------------------------------------------------------------------------------------

TEST_SUITE("query text rendering")
{
	TEST_CASE("the colon sits in the same column at every depth")
	{
		const auto tree = section("One", {section("Two", {leaf("Leaf", "x")}), leaf("Direct", "y")});
		const auto lines = linesOf(rendered(tree));
		REQUIRE(lines.size() == 4);
		CHECK(lines[0] == "One");
		CHECK(lines[1] == "    Two");
		CHECK(lines[2].find(": ") == KEY_COLUMN);
		CHECK(lines[2].ends_with(": x"));
		CHECK(lines[3].find(": ") == KEY_COLUMN);
		CHECK(lines[3].ends_with(": y"));
		CHECK(lines[2].starts_with("        Leaf"));
	}

	TEST_CASE("a label that reaches the colon column is followed by one space")
	{
		const std::string label(KEY_COLUMN - 2 * INDENT_WIDTH, 'L');
		const auto lines = linesOf(rendered(leaf(label, "v"), 2));
		REQUIRE(lines.size() == 1);
		CHECK(lines[0] == std::string(2 * INDENT_WIDTH, ' ') + label + " : v");
	}

	TEST_CASE("a missing value prints as N/A without its unit")
	{
		CHECK(rendered(leaf("Fan Speed", std::nullopt, "%")).find(": N/A\n") != std::string::npos);
	}

	TEST_CASE("a unit follows the value after a space")
	{
		CHECK(rendered(leaf("Power Draw", "66.12", "W")).ends_with(": 66.12 W\n"));
	}

	TEST_CASE("a value with a unit is shown to two decimals, a value without one is untouched")
	{
		CHECK(rendered(leaf("Used", "218.62109375", "MiB")).ends_with(": 218.62 MiB\n"));
		CHECK(rendered(leaf("Voltage", "1.3515625", "V")).ends_with(": 1.35 V\n"));
		CHECK(rendered(leaf("Total", "239", "MiB")).ends_with(": 239 MiB\n"));
		CHECK(rendered(leaf("Draw", "12.5", "W")).ends_with(": 12.5 W\n"));
		CHECK(rendered(leaf("Name", "0.123456")).ends_with(": 0.123456\n"));
		CHECK(rendered(leaf("Mode", "1.2.3.4", "x")).ends_with(": 1.2.3.4 x\n"));
	}

	TEST_CASE("an empty list prints its title and None")
	{
		const auto lines = linesOf(rendered(processesSection({})));
		REQUIRE(lines.size() == 2);
		CHECK(lines[0] == "Processes");
		CHECK(lines[1] == "    None");
	}

	TEST_CASE("a record prints as a leaf with its fields one level deeper")
	{
		const psInfo proc{.processId = 4242,
						  .commandName = "app",
						  .devId = 0,
						  .engines = static_cast<uint64_t>(ZES_ENGINE_TYPE_FLAG_COMPUTE),
						  .sharedSize = 2048,
						  .memSize = 206 * 1024,
						  .util = {}};
		const auto lines = linesOf(rendered(processesSection(std::span{&proc, 1})));
		REQUIRE(lines.size() == 6);
		CHECK(lines[0] == "Processes");
		CHECK(lines[1].starts_with("    Process ID"));
		CHECK(lines[1].ends_with(": 4242"));
		CHECK(lines[2].starts_with("        Type"));
		CHECK(lines[2].ends_with(": C"));
		CHECK(lines[3].ends_with(": app"));
		CHECK(lines[4].ends_with(": 206 MiB"));
		CHECK(lines[5].ends_with(": 2 MiB"));
	}

	TEST_CASE("the report has a banner, a header block and one block per device")
	{
		QueryReport report;
		report.timestamp = "2026-10-21T10:00:00Z";
		report.umdVersion = "1.3";
		auto first = section("GPU 0000:03:00.0", {leaf("Product Name", "A")});
		first.value = "0000:03:00.0";
		auto second = section("GPU 0000:04:00.0", {leaf("Product Name", "B")});
		second.value = "0000:04:00.0";
		report.gpus = {first, second};

		const auto text = renderText(report);
		const auto lines = linesOf(text);
		REQUIRE(lines.size() > 8);
		CHECK(lines[0] == LOG_BANNER);
		CHECK(lines[1].empty());
		CHECK(lines[2].starts_with("Timestamp"));
		CHECK(lines[2].ends_with(": 2026-10-21T10:00:00Z"));
		CHECK(lines[3].ends_with(": 1.3"));
		CHECK(lines[4].ends_with(": N/A"));
		CHECK(text.find("Attached GPUs") != std::string::npos);
		CHECK(text.find("Attached GPUs                             : 2\n") != std::string::npos);
		CHECK(countOf(text, "\nGPU 0000:") == 2);
		CHECK(countOf(text, "\n\nGPU 0000:04:00.0\n") == 1);
		CHECK(text.ends_with("\n"));
	}
}

// ---------------------------------------------------------------------------------------
// JSON
// ---------------------------------------------------------------------------------------

TEST_SUITE("query json")
{
	TEST_CASE("a missing value is null and a present one keeps its full precision")
	{
		const auto tree = section("S", {leaf("A", std::nullopt), leaf("B", "218.62109375", "MiB")});
		const auto json = nodeToJson(tree);
		CHECK(json["A"].is_null());
		CHECK(json["B"] == "218.62109375");
	}

	TEST_CASE("an empty list is an empty array and records are objects led by their own key")
	{
		CHECK(nodeToJson(processesSection({})).is_array());
		CHECK(nodeToJson(processesSection({})).empty());

		const psInfo proc{.processId = 7,
						  .commandName = "app",
						  .devId = 0,
						  .engines = 0,
						  .sharedSize = 0,
						  .memSize = 3 * 1024,
						  .util = {}};
		const auto json = nodeToJson(processesSection(std::span{&proc, 1}));
		REQUIRE(json.size() == 1);
		CHECK(json[0].begin().key() == "process_id");
		CHECK(json[0]["process_id"] == "7");
		CHECK(json[0]["name"] == "app");
		CHECK(json[0]["used_gpu_memory"] == "3");
	}

	TEST_CASE("the report carries its header, the device count and each device keyed by PCI address")
	{
		QueryReport report;
		report.timestamp = "t";
		report.kmdVersion = "k";
		auto total = leaf("Total", "1");
		total.key = "total";
		auto memory = section("Memory Usage", {total});
		memory.key = "memory_usage";
		auto gpu = section("GPU 0000:03:00.0", {memory});
		gpu.key = "pci_bus_id";
		gpu.value = "0000:03:00.0";
		report.gpus = {gpu};

		const auto json = toJson(report);
		CHECK(json["timestamp"] == "t");
		CHECK(json["kmd_version"] == "k");
		CHECK(json["umd_version"].is_null());
		CHECK(json["attached_gpus"] == 1);
		REQUIRE(json["gpus"].size() == 1);
		CHECK(json["gpus"][0].begin().key() == "pci_bus_id");
		CHECK(json["gpus"][0]["pci_bus_id"] == "0000:03:00.0");
		CHECK(json["gpus"][0]["memory_usage"]["total"] == "1");
	}
}

// ---------------------------------------------------------------------------------------
// The report layout
// ---------------------------------------------------------------------------------------

TEST_SUITE("query layout")
{
	TEST_CASE("every registry field the report reads exists, once")
	{
		const auto names = referencedMetricNames();
		REQUIRE_FALSE(names.empty());
		for (const auto name : names) {
			CHECK_MESSAGE(metrics::findMetric(name).has_value(), name);
		}
		for (std::size_t i = 0; i < names.size(); ++i) {
			CHECK_MESSAGE(std::ranges::count(names, names[i]) == 1, names[i]);
		}
	}

	TEST_CASE("every static property is shown exactly once and every field exactly once")
	{
		const MarkedInputs inputs;
		const auto device = buildDeviceNode("0000:03:00.0", QuerySelection{}, inputs.metricValues, inputs.statics, {});
		const auto text = rendered(device);
		// device_state shows its own marker like the rest
		CHECK(countOf(text, ": S:") == staticKeys().size() - 1);
		CHECK(countOf(text, ": M:") == referencedMetricNames().size());
		CHECK(text.find(": 1.00 MiB") != std::string::npos);
		for (const auto name : referencedMetricNames()) {
			CHECK_MESSAGE(countOf(text, ": M:" + std::string{name}) >= 1, name);
		}
	}

	TEST_CASE("the sections appear in report order with the process list last")
	{
		const MarkedInputs inputs;
		const auto device = buildDeviceNode("0000:03:00.0", QuerySelection{}, inputs.metricValues, inputs.statics, {});
		const auto labels = topLevelLabels(device);
		const auto position = [&labels](std::string_view label) {
			const auto it = std::ranges::find(labels, label);
			REQUIRE(it != labels.end());
			return it - labels.begin();
		};
		CHECK(position("Driver") < position("Firmware"));
		CHECK(position("Firmware") < position("Compute Resources"));
		CHECK(position("Compute Resources") < position("PCI"));
		CHECK(position("PCI") < position("Memory Usage"));
		CHECK(position("Memory Usage") < position("Utilization"));
		CHECK(position("Utilization") < position("ECC Mode"));
		CHECK(position("ECC Mode") < position("ECC Errors"));
		CHECK(position("ECC Errors") < position("Temperature"));
		CHECK(position("Temperature") < position("Power Readings"));
		CHECK(position("Power Readings") < position("Clocks"));
		CHECK(position("Clocks") < position("Max Clocks"));
		CHECK(position("Max Clocks") < position("Min Clocks"));
		CHECK(position("Min Clocks") < position("Frequency Range"));
		CHECK(position("Frequency Range") < position("Supported Clocks"));
		CHECK(position("Fan Speed") < position("Fan Mode"));
		CHECK(position("Fan Mode") < position("Clocks Event Reasons"));
		CHECK(position("Clocks Event Reasons") < position("Standby Mode"));
		CHECK(position("Standby Mode") < position("Memory Usage"));
		CHECK(position("ECC Errors") < position("Page Retirement"));
		CHECK(position("Page Retirement") < position("Temperature"));
		CHECK(labels.back() == "Processes");
	}

	TEST_CASE("the clocks event reasons section has one row per throttle reason, in table order")
	{
		const MarkedInputs inputs;
		const auto device = buildDeviceNode("0000:03:00.0", QuerySelection{}, inputs.metricValues, inputs.statics, {});
		const auto it = std::ranges::find(device.children, "Clocks Event Reasons", &QueryNode::label);
		REQUIRE(it != device.children.end());
		REQUIRE(it->children.size() == metrics::clock::THROTTLE_REASONS.size());
		for (std::size_t i = 0; i < it->children.size(); ++i) {
			CHECK(it->children[i].label == metrics::clock::THROTTLE_REASONS[i].label);
			CHECK(it->children[i].key == metrics::clock::THROTTLE_REASONS[i].key);
			CHECK(it->children[i].value == "M:" + std::string{metrics::clock::THROTTLE_REASONS[i].metricName});
		}
	}

	TEST_CASE("the scheduler, health and link rows sit in their sections")
	{
		const MarkedInputs inputs;
		const auto device = buildDeviceNode("0000:03:00.0", QuerySelection{}, inputs.metricValues, inputs.statics, {});
		const auto text = rendered(device);
		const auto lines = linesOf(text);
		const auto indexOf = [&lines](std::string_view label) {
			const auto it = std::ranges::find_if(
				lines, [label](const std::string &l) { return l.find(label) != std::string::npos; });
			REQUIRE(it != lines.end());
			return it - lines.begin();
		};
		CHECK(indexOf("Device State") < indexOf("Device Health"));
		CHECK(indexOf("Max Command Queue Priority") < indexOf("Scheduler Mode"));
		CHECK(indexOf("Scheduler Mode") < indexOf("    PCI"));
		CHECK(indexOf("Max Bandwidth") < indexOf("Link Status"));
		CHECK(indexOf("Composite Temp") < indexOf("GPU Throttle Temp"));
		CHECK(indexOf("Memory Shutdown Temp") < indexOf("    Power Readings"));
	}

	TEST_CASE("the device node carries its PCI address")
	{
		const auto device = buildDeviceNode("0000:03:00.0", QuerySelection{}, {}, {}, {});
		CHECK(device.label == "GPU 0000:03:00.0");
		CHECK(device.key == "pci_bus_id");
		CHECK(device.value == "0000:03:00.0");
	}

	TEST_CASE("selecting one section shows only that section")
	{
		const MarkedInputs inputs;
		const auto device =
			buildDeviceNode("0000:03:00.0", *parseSelection("MEMORY"), inputs.metricValues, inputs.statics, {});
		CHECK(topLevelLabels(device) == std::vector<std::string>{"Memory Usage"});
	}

	TEST_CASE("selecting PIDS shows only the process list")
	{
		const MarkedInputs inputs;
		const auto device =
			buildDeviceNode("0000:03:00.0", *parseSelection("PIDS"), inputs.metricValues, inputs.statics, {});
		CHECK(topLevelLabels(device) == std::vector<std::string>{"Processes"});
	}

	TEST_CASE("a row with a shared field is shown when either of its sections is selected")
	{
		const MarkedInputs inputs;
		for (const char *sections : {"UTILIZATION", "EU_ARRAY"}) {
			const auto device =
				buildDeviceNode("0000:03:00.0", *parseSelection(sections), inputs.metricValues, inputs.statics, {});
			CHECK_MESSAGE(rendered(device).find("EU Array") != std::string::npos, sections);
		}
	}

	TEST_CASE("memory utilization appears under both MEMORY and UTILIZATION")
	{
		const MarkedInputs inputs;
		for (const char *sections : {"MEMORY", "UTILIZATION"}) {
			const auto device =
				buildDeviceNode("0000:03:00.0", *parseSelection(sections), inputs.metricValues, inputs.statics, {});
			CHECK_MESSAGE(rendered(device).find("M:utilization.memory") != std::string::npos, sections);
		}
	}

	TEST_CASE("a property that does not apply to the device is left out, one that failed reads N/A")
	{
		QueryStatics statics;
		statics.values.push_back({"stepping", std::nullopt});
		const auto device = buildDeviceNode("0000:03:00.0", *parseSelection("IDENTITY"), {}, statics, {});
		const auto text = rendered(device);
		CHECK(text.find("Stepping") != std::string::npos);
		CHECK(text.find("SKU Type") == std::string::npos);
		CHECK(text.find("Part Number") == std::string::npos);
	}

	TEST_CASE("the PCI address is also shown as its four fields")
	{
		MetricValues values;
		values["pci.bus_id"] = "00a1:3c:1f.2";
		const auto text = rendered(buildDeviceNode("x", *parseSelection("PCI"), values, {}, {}));
		CHECK(text.find(": 0x3c\n") != std::string::npos);
		CHECK(text.find(": 0x1f\n") != std::string::npos);
		CHECK(text.find(": 0x00a1\n") != std::string::npos);
		CHECK(text.find(": 0x2\n") != std::string::npos);
		CHECK(text.find(": 00a1:3c:1f.2\n") != std::string::npos);
	}

	TEST_CASE("the four PCI address fields read N/A when the address is unusable")
	{
		MetricValues values;
		values["pci.bus_id"] = "junk";
		const auto text = rendered(buildDeviceNode("x", *parseSelection("PCI"), values, {}, {}));
		CHECK(text.find("Bus                               : N/A\n") != std::string::npos);
		CHECK(text.find("Domain                            : N/A\n") != std::string::npos);
	}

	TEST_CASE("the driver's unknown link values read N/A")
	{
		MetricValues values;
		values["pcie.link.gen.max"] = "-1";
		values["pcie.link.width.current"] = "8";
		const auto text = rendered(buildDeviceNode("x", *parseSelection("PCI"), values, {}, {}));
		CHECK(text.find("Max                       : N/A\n") != std::string::npos);
		CHECK(text.find("Current                   : 8\n") != std::string::npos);
	}

	TEST_CASE("a value the platform left empty reads N/A")
	{
		MetricValues values;
		values["vbios_version"] = "";
		QueryStatics statics;
		statics.values.push_back({"kernel_version", std::string{}});
		const auto text = rendered(buildDeviceNode("x", *parseSelection("IDENTITY"), values, statics, {}));
		CHECK(text.find("GFX Firmware Version              : N/A\n") != std::string::npos);
		CHECK(text.find("Kernel Version                    : N/A\n") != std::string::npos);
	}

	TEST_CASE("a getter that reports the text N/A is shown as N/A without its unit, and as null in JSON")
	{
		MetricValues values;
		values["memory.read.bandwidth"] = "N/A";
		const auto device = buildDeviceNode("x", *parseSelection("MEMORY"), values, {}, {});
		CHECK(rendered(device).find("Read Bandwidth                    : N/A\n") != std::string::npos);

		QueryReport report;
		report.gpus = {device};
		CHECK(toJson(report)["gpus"][0]["memory_usage"]["read_bandwidth"].is_null());
	}

	TEST_CASE("the largest allocation is converted from bytes to MiB")
	{
		QueryStatics statics;
		statics.values.push_back({"max_mem_alloc_size", std::string{"12168933376"}});
		const auto text = rendered(buildDeviceNode("x", *parseSelection("MEMORY"), {}, statics, {}));
		CHECK(text.find(": 11605.20 MiB\n") != std::string::npos);

		statics.values.front().value = "not a number";
		CHECK(rendered(buildDeviceNode("x", *parseSelection("MEMORY"), {}, statics, {}))
				  .find("Max Memory Alloc Size             : N/A\n") != std::string::npos);
	}

	TEST_CASE("a survivability device shows only its state and recovery action")
	{
		const auto device = survivabilityNode("0000:05:00.0");
		const auto lines = linesOf(rendered(section("GPU", device.children)));
		REQUIRE(lines.size() == 3);
		CHECK(lines[1].ends_with(": survivability mode"));
		CHECK(lines[2].ends_with(": Firmware update and GPU reset"));
		CHECK(device.value == "0000:05:00.0");
	}
}

// ---------------------------------------------------------------------------------------
// A device that answers nothing
// ---------------------------------------------------------------------------------------

TEST_CASE("a device that answers nothing renders every row as N/A and every value as null")
{
	MetricValues values;
	for (const auto name : referencedMetricNames()) {
		values[name] = std::nullopt;
	}
	QueryStatics statics;
	for (const auto &known : staticKeys()) {
		statics.values.push_back({known.key, std::nullopt});
	}

	const auto node = buildDeviceNode("0000:00:00.0", QuerySelection{}, values, statics, {});
	std::string text;
	for (const auto &child : node.children) {
		renderNode(child, 1, text);
	}
	std::size_t leaves = 0;
	for (const auto &line : linesOf(text)) {
		if (line.find(": ") != std::string::npos) {
			++leaves;
			CHECK_MESSAGE(line.ends_with(": N/A"), line);
		}
	}
	CHECK(leaves > referencedMetricNames().size());

	QueryReport report;
	report.gpus = {node};
	CHECK_NOTHROW((void)renderText(report));
	const auto json = toJson(report).dump(4);
	CHECK(json.find("\"N/A\"") == std::string::npos);
}

TEST_CASE("selecting nothing but processes reads no registry field")
{
	CHECK(selectedMetrics(*parseSelection("PIDS")).empty());
}

TEST_CASE("selecting one section reads only that section's fields")
{
	const auto fields = selectedMetrics(*parseSelection("TEMPERATURE"));
	REQUIRE_FALSE(fields.empty());
	for (const auto *field : fields) {
		CHECK_MESSAGE(metrics::hasGroup(field->groups, metrics::MetricGroup::TEMPERATURE), field->name);
	}
}

// ---------------------------------------------------------------------------------------
// Command line
// ---------------------------------------------------------------------------------------

TEST_SUITE("query command line")
{
	[[nodiscard]] QueryOptions parsedOptions(std::initializer_list<const char *> argv)
	{
		FakeArgs fake{argv};
		auto parsed = cmdQuery::parseOptions(&fake.args);
		REQUIRE(std::holds_alternative<QueryOptions>(parsed));
		return std::get<QueryOptions>(std::move(parsed));
	}

	[[nodiscard]] int rejectedWith(std::initializer_list<const char *> argv)
	{
		FakeArgs fake{argv};
		auto parsed = cmdQuery::parseOptions(&fake.args);
		REQUIRE(std::holds_alternative<int>(parsed));
		return std::get<int>(parsed);
	}

	TEST_CASE("no options asks for one report of everything")
	{
		const auto opts = parsedOptions({"xpu-smi", "query"});
		CHECK(opts.selection.groups == metrics::MetricGroup::ALL);
		CHECK(opts.selection.processes);
		CHECK(opts.deviceSpec.empty());
		CHECK_FALSE(opts.json);
		CHECK(opts.loop.count() == 0);
		CHECK(opts.count == 0);
		CHECK_FALSE(opts.file.has_value());
	}

	TEST_CASE("every option is read")
	{
		const auto opts = parsedOptions({"xpu-smi", "query", "--display", "MEMORY,ECC", "--id", "0,1", "--json",
										 "--loop", "2", "--count", "3", "-f", "/tmp/report.txt"});
		CHECK(opts.selection.groups == (metrics::MetricGroup::MEMORY | metrics::MetricGroup::ECC));
		CHECK_FALSE(opts.selection.processes);
		CHECK(opts.deviceSpec == "0,1");
		CHECK(opts.json);
		CHECK(opts.loop == std::chrono::seconds{2});
		CHECK(opts.count == 3);
		CHECK(opts.file == "/tmp/report.txt");
	}

	TEST_CASE("the equals form and the device aliases work")
	{
		const auto opts = parsedOptions({"xpu-smi", "query", "--display=POWER", "--device=0000:03:00.0", "-j"});
		CHECK(opts.selection.groups == metrics::MetricGroup::POWER);
		CHECK(opts.deviceSpec == "0000:03:00.0");
		CHECK(opts.json);
		CHECK(parsedOptions({"xpu-smi", "query", "-d", "1"}).deviceSpec == "1");
	}

	TEST_CASE("milliseconds override seconds")
	{
		CHECK(parsedOptions({"xpu-smi", "query", "--loop", "2", "--loop-ms", "500"}).loop ==
			  std::chrono::milliseconds{500});
		CHECK(parsedOptions({"xpu-smi", "query", "--loop-ms", "500"}).loop == std::chrono::milliseconds{500});
	}

	TEST_CASE("a frame limit without an interval loops once a second")
	{
		const auto opts = parsedOptions({"xpu-smi", "query", "--count", "3"});
		CHECK(opts.loop == std::chrono::seconds{1});
		CHECK(opts.count == 3);
	}

	TEST_CASE("one report is never a loop")
	{
		const auto opts = parsedOptions({"xpu-smi", "query", "--loop", "5", "--count", "1"});
		CHECK(opts.loop.count() == 0);
		CHECK(parsedOptions({"xpu-smi", "query", "--count", "1"}).loop.count() == 0);
	}

	TEST_CASE("device -1 selects every device")
	{
		CHECK(parsedOptions({"xpu-smi", "query", "--device", "-1"}).deviceSpec.empty());
	}

	TEST_CASE("bad values are rejected with an invalid-argument code")
	{
		constexpr int invalid = static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT);
		CHECK(rejectedWith({"xpu-smi", "query", "--loop", "0"}) == invalid);
		CHECK(rejectedWith({"xpu-smi", "query", "--loop", "-3"}) == invalid);
		CHECK(rejectedWith({"xpu-smi", "query", "--loop", "abc"}) == invalid);
		CHECK(rejectedWith({"xpu-smi", "query", "--loop-ms", "0"}) == invalid);
		CHECK(rejectedWith({"xpu-smi", "query", "--count", "0"}) == invalid);
		CHECK(rejectedWith({"xpu-smi", "query", "--display", "BOGUS"}) == invalid);
		CHECK(rejectedWith({"xpu-smi", "query", "--bogus"}) == invalid);
	}

	TEST_CASE("a trailing comma in the section list is tolerated")
	{
		CHECK(parsedOptions({"xpu-smi", "query", "--display", "MEMORY,"}).selection.groups ==
			  metrics::MetricGroup::MEMORY);
	}

	TEST_CASE("help is a successful early exit")
	{
		CHECK(rejectedWith({"xpu-smi", "query", "--help"}) == static_cast<int>(ZE_RESULT_SUCCESS));
		CHECK(rejectedWith({"xpu-smi", "query", "-h"}) == static_cast<int>(ZE_RESULT_SUCCESS));
	}

	TEST_CASE("only the help word and help flags skip the driver")
	{
		cmdQuery command;
		const auto driverFor = [&command](std::initializer_list<const char *> tokens) {
			std::vector<char *> argv;
			for (const char *t : tokens) {
				argv.push_back(const_cast<char *>(t));
			}
			return command.requiredDriver(argv);
		};
		CHECK(driverFor({"help"}) == DriverMode::None);
		CHECK(driverFor({"-h"}) == DriverMode::None);
		CHECK(driverFor({"--help"}) == DriverMode::None);
		CHECK(driverFor({}) == DriverMode::Full);
		CHECK(driverFor({"--json"}) == DriverMode::Full);
		CHECK(driverFor({"--display", "MEMORY"}) == DriverMode::Full);
	}

	TEST_CASE("the command is named query and its help prints")
	{
		cmdQuery command;
		CHECK(command.getName() == "query");
		CHECK_NOTHROW(command.help());
		CHECK_NOTHROW(command.help(SHORT_HELP));
	}
}
