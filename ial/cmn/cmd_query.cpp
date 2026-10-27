/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "cmd_query.h"
#include "cmd_discovery.h"
#include "debug.h"
#include "driver_util.h"
#include "frame_loop.h"
#include "metrics/clock.h"
#include "table_builder.h"
#include "terminal.h"
#include "utility/compat/format.h"
#include "utility/logger/logger.h"
#include <CLI/CLI.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <cctype>
#include <future>
#include <limits>
#include <ios>
#include <iterator>
#include <memory>
#include <ranges>
#include <system_error>
#include <thread>
#include <utility>

namespace query {

namespace {

using metrics::MetricGroup;

// ---------------------------------------------------------------------------------------
// Static properties
// ---------------------------------------------------------------------------------------

/// Works a static property out from the device itself; nothing when the device cannot tell.
using ComputedStatic = std::optional<std::string> (*)(devInfo &dev);

[[nodiscard]] std::optional<std::string> deviceStateOf(devInfo &dev)
{
	return std::string{dev.dev->isInSurvMode() ? DEVICE_STATE_SURV_MODE : DEVICE_STATE_NORMAL};
}

/// The number of the device's DRM card node, e.g. 1 for /dev/dri/card1.
[[nodiscard]] std::optional<std::string> minorNumberOf(devInfo &dev)
{
	constexpr std::string_view cardPrefix = "card";
	const std::string path = dev.dev->getDrmDevPath();
	const auto at = path.rfind(cardPrefix);
	if (at == std::string::npos) {
		return std::nullopt;
	}
	const std::string_view number = std::string_view{path}.substr(at + cardPrefix.size());
	if (number.empty() || !std::ranges::all_of(number, [](char c) { return c >= '0' && c <= '9'; })) {
		return std::nullopt;
	}
	return std::string{number};
}

/// Whether the device is a physical function or an SR-IOV virtual function, as discovery says it.
[[nodiscard]] std::optional<std::string> virtualizationModeOf(devInfo &dev)
{
	auto *p = dev.dev->getPCI();
	if (p == nullptr) {
		return std::nullopt;
	}
	return std::string{p->getFuncType() == DEVICE_FUNCTION_TYPE_VIRTUAL ? "virtual" : "physical"};
}

/** One static property: where its text comes from and which section shows it. */
struct StaticSpec
{
	std::string_view key;
	MetricGroup group;
	discoverySubCmdFunc getter; ///< Discovery getter that reads it, or nullptr when @c compute does.
	ComputedStatic compute;		///< Works it out from the device, or nullptr when @c getter reads it.
	bool amcOnly;				///< Only devices with an AMC have it.
};

// clang-format off
constexpr auto STATIC_TABLE = std::to_array<StaticSpec>({
	{"product_brand",       MetricGroup::IDENTITY, &cmdDiscovery::vendorName, nullptr,               false},
	{"minor_number",        MetricGroup::IDENTITY, nullptr,                   &minorNumberOf,        false},
	{"device_state",        MetricGroup::IDENTITY, nullptr,                   &deviceStateOf,        false},
	{"part_number",         MetricGroup::IDENTITY, &cmdDiscovery::partNumber, nullptr,               true},
	{"virtualization_mode", MetricGroup::IDENTITY, nullptr,                   &virtualizationModeOf, false},
	{"pci_slot",            MetricGroup::PCI,      &cmdDiscovery::pciSlot,    nullptr,               false},
	{"stepping",            MetricGroup::IDENTITY, &cmdDiscovery::stepping,   nullptr,               false},
	{"number_of_tiles",     MetricGroup::COMPUTE,  &cmdDiscovery::numberOfTiles, nullptr,            false},
	{"number_of_slices",    MetricGroup::COMPUTE,  &cmdDiscovery::numberOfSlices, nullptr,           false},
	{"sub_slices_per_slice", MetricGroup::COMPUTE, &cmdDiscovery::numberOfSubslicesPerSlice, nullptr, false},
	{"number_of_eus",       MetricGroup::COMPUTE,  &cmdDiscovery::eus,        nullptr,               false},
	{"memory_type",         MetricGroup::MEMORY,   &cmdDiscovery::memoryType, nullptr,               false},
	{"memory_channels",     MetricGroup::MEMORY,   &cmdDiscovery::memoryChannels, nullptr,           false},
	{"memory_bus_width",    MetricGroup::MEMORY,   &cmdDiscovery::memoryBusWidth, nullptr,           false},
});
// clang-format on

[[nodiscard]] const StaticSpec *staticSpecOf(std::string_view key) noexcept
{
	const auto it = std::ranges::find(STATIC_TABLE, key, &StaticSpec::key);
	return it == STATIC_TABLE.end() ? nullptr : &*it;
}

// ---------------------------------------------------------------------------------------
// Registry fields
// ---------------------------------------------------------------------------------------

[[nodiscard]] const metrics::QueryMetric *registered(std::string_view name)
{
	static const std::unordered_map<std::string_view, const metrics::QueryMetric *> BY_NAME = [] {
		std::unordered_map<std::string_view, const metrics::QueryMetric *> byName;
		for (const auto &field : metrics::getQueryMetrics()) {
			byName.emplace(field.name, &field);
		}
		return byName;
	}();
	const auto it = BY_NAME.find(name);
	return it == BY_NAME.end() ? nullptr : it->second;
}

/// An empty text, or the text N/A that some getters report instead of failing, is a value nobody could supply.
[[nodiscard]] std::optional<std::string> presentOrNull(std::optional<std::string> value)
{
	if (value && (value->empty() || *value == NOT_AVAILABLE)) {
		return std::nullopt;
	}
	return value;
}

// ---------------------------------------------------------------------------------------
// The report layout
// ---------------------------------------------------------------------------------------

enum class SpecKind
{
	Section,
	Field,
	Static,
	BdfDomain,
	BdfBus,
	BdfDevice,
};

/**
 * Describes one row or section of the device report.
 *
 * The layout is a flat list in report order. The rows that follow a section one level deeper,
 * up to the next row at the section's depth or shallower, are its children.
 */
struct Spec
{
	SpecKind kind = SpecKind::Section;
	std::size_t depth = 0;
	std::string_view label;
	std::string_view key;
	std::string_view source;		///< Registry field name, or static property key.
	bool negativeIsUnknown = false; ///< A negative reading is the driver's way of saying "unknown".
	bool hideWhenAbsent = false;	///< See QueryNode::hideWhenAbsent.
	bool zeroIsUnknown = false;		///< A reading of 0 is the driver's way of saying "unknown".
};

constexpr std::string_view BUS_ID_FIELD = "pci.bus_id";

[[nodiscard]] constexpr Spec sectionOf(std::size_t depth, std::string_view label, std::string_view key)
{
	return Spec{SpecKind::Section, depth, label, key, {}};
}

[[nodiscard]] constexpr Spec fieldOf(std::size_t depth, std::string_view label, std::string_view key,
									 std::string_view name)
{
	return Spec{SpecKind::Field, depth, label, key, name};
}

/// A field whose driver reports -1 when it does not know the value.
[[nodiscard]] constexpr Spec linkFieldOf(std::size_t depth, std::string_view label, std::string_view key,
										 std::string_view name)
{
	return Spec{SpecKind::Field, depth, label, key, name, true};
}

/// A field only some devices or drivers supply: the text report leaves it out where it has no value.
[[nodiscard]] constexpr Spec optionalFieldOf(std::size_t depth, std::string_view label, std::string_view key,
											 std::string_view name)
{
	return Spec{SpecKind::Field, depth, label, key, name, false, true};
}

/// A field whose driver reports 0 when it does not know the value, e.g. the memory clock on CRI.
[[nodiscard]] constexpr Spec zeroUnknownFieldOf(std::size_t depth, std::string_view label, std::string_view key,
												std::string_view name)
{
	return Spec{SpecKind::Field, depth, label, key, name, false, false, true};
}

[[nodiscard]] constexpr Spec staticOf(std::size_t depth, std::string_view label, std::string_view key,
									  std::string_view staticKey)
{
	return Spec{SpecKind::Static, depth, label, key, staticKey};
}

[[nodiscard]] constexpr Spec pciPartOf(SpecKind kind, std::size_t depth, std::string_view label, std::string_view key)
{
	return Spec{kind, depth, label, key, BUS_ID_FIELD};
}

// Identity first, then one section per subsystem. Values that need extra GSC or AMC round trips
// per device are left out, as are rows that no current device, driver and kernel gives useful
// values for (PCIe replays, power limit range, pending ECC state, offline pages, fan speed).
// clang-format off
constexpr auto LAYOUT_HEAD = std::to_array<Spec>({
	fieldOf(0, "Product Name", "product_name", "name"),
	staticOf(0, "Product Brand", "product_brand", "product_brand"),
	staticOf(0, "Stepping", "stepping", "stepping"),
	fieldOf(0, "Serial Number", "serial_number", "serial"),
	fieldOf(0, "GPU UUID", "gpu_uuid", "uuid"),
	fieldOf(0, "Device Index", "device_index", "index"),
	staticOf(0, "Minor Number", "minor_number", "minor_number"),
	fieldOf(0, "GFX Firmware Version", "gfx_firmware_version", "vbios_version"),
	staticOf(0, "Board Part Number", "board_part_number", "part_number"),
	sectionOf(0, "Platform Info", "platform_info"),
		staticOf(1, "Slot Number", "slot_number", "pci_slot"),
	staticOf(0, "Device State", "device_state", "device_state"),
	sectionOf(0, "GPU Virtualization Mode", "gpu_virtualization_mode"),
		staticOf(1, "Virtualization Mode", "virtualization_mode", "virtualization_mode"),
	fieldOf(0, "GPU Recovery Action", "gpu_recovery_action", "recovery_action"),
	sectionOf(0, "PCI", "pci"),
		pciPartOf(SpecKind::BdfBus, 1, "Bus", "bus"),
		pciPartOf(SpecKind::BdfDevice, 1, "Device", "device"),
		pciPartOf(SpecKind::BdfDomain, 1, "Domain", "domain"),
		fieldOf(1, "Base Classcode", "base_classcode", "pci.base_class"),
		fieldOf(1, "Sub Classcode", "sub_classcode", "pci.sub_class"),
		fieldOf(1, "Device Id", "device_id", "pci.device_id"),
		fieldOf(1, "Bus Id", "bus_id", BUS_ID_FIELD),
		fieldOf(1, "Sub System Id", "sub_system_id", "pci.sub_device_id"),
		sectionOf(1, "GPU Link Info", "gpu_link_info"),
			sectionOf(2, "PCIe Generation", "pcie_generation"),
				linkFieldOf(3, "Max", "max", "pcie.link.gen.max"),
				linkFieldOf(3, "Current", "current", "pcie.link.gen.current"),
				optionalFieldOf(3, "Host Max", "host_max", "pcie.link.gen.host_max"),
			sectionOf(2, "Link Width", "link_width"),
				linkFieldOf(3, "Max", "max", "pcie.link.width.max"),
				linkFieldOf(3, "Current", "current", "pcie.link.width.current"),
				optionalFieldOf(3, "Host Max", "host_max", "pcie.link.width.host_max"),
		sectionOf(1, "PCIe Errors", "pcie_errors"),
			optionalFieldOf(2, "Correctable", "correctable", "pcie.errors.correctable"),
			optionalFieldOf(2, "Non-Fatal", "non_fatal", "pcie.errors.non_fatal"),
			optionalFieldOf(2, "Fatal", "fatal", "pcie.errors.fatal"),
			optionalFieldOf(2, "Replay Number Rollovers", "replay_number_rollovers", "pcie.errors.replay_rollovers"),
			optionalFieldOf(2, "Replay Timer Timeouts", "replay_timer_timeouts", "pcie.errors.replay_timeouts"),
		fieldOf(1, "Tx Throughput", "tx_throughput", "pcie.tx.throughput.kbs"),
		fieldOf(1, "Rx Throughput", "rx_throughput", "pcie.rx.throughput.kbs"),
		optionalFieldOf(1, "Atomic Caps Inbound", "atomic_caps_inbound", "pcie.atomic.completer"),
		optionalFieldOf(1, "Atomic Requests", "atomic_requests", "pcie.atomic.requests"),
	sectionOf(0, "Clocks Event Reasons", "clocks_event_reasons"),
});

// The rows of Clocks Event Reasons come from THROTTLE_REASONS, between LAYOUT_HEAD and LAYOUT_TAIL.

constexpr auto LAYOUT_TAIL = std::to_array<Spec>({
	sectionOf(0, "FB Memory Usage", "fb_memory_usage"),
		fieldOf(1, "Total", "total", "memory.total"),
		fieldOf(1, "Reserved", "reserved", "memory.reserved"),
		fieldOf(1, "Used", "used", "memory.used"),
		fieldOf(1, "Free", "free", "memory.free"),
	sectionOf(0, "BAR Memory", "bar_memory"),
		fieldOf(1, "Total", "total", "pci.bar.memory.size"),
		optionalFieldOf(1, "Resizable BAR", "resizable_bar", "pci.bar.resizable"),
	sectionOf(0, "Memory Info", "memory_info"),
		staticOf(1, "Memory Type", "memory_type", "memory_type"),
		staticOf(1, "Number of Memory Channels", "number_of_memory_channels", "memory_channels"),
		staticOf(1, "Memory Bus Width", "memory_bus_width", "memory_bus_width"),
	fieldOf(0, "Scheduler Mode", "scheduler_mode", "scheduler.mode"),
	sectionOf(0, "Compute Resources", "compute_resources"),
		staticOf(1, "Number of Tiles", "number_of_tiles", "number_of_tiles"),
		staticOf(1, "Number of Slices", "number_of_slices", "number_of_slices"),
		staticOf(1, "Number of Sub-slices per Slice", "number_of_sub_slices_per_slice", "sub_slices_per_slice"),
		staticOf(1, "Number of EUs", "number_of_eus", "number_of_eus"),
	sectionOf(0, "Utilization", "utilization"),
		fieldOf(1, "GPU", "gpu", "utilization.gpu"),
		fieldOf(1, "Memory", "memory", "utilization.memory"),
		fieldOf(1, "Compute", "compute", "utilization.compute"),
		fieldOf(1, "Render", "render", "utilization.render"),
		fieldOf(1, "Media", "media", "utilization.media"),
		fieldOf(1, "Copy", "copy", "utilization.copy"),
	sectionOf(0, "ECC Mode", "ecc_mode"),
		fieldOf(1, "Current", "current", "ecc.mode.current"),
	sectionOf(0, "ECC Errors", "ecc_errors"),
		sectionOf(1, "Aggregate", "aggregate"),
			fieldOf(2, "Correctable", "correctable", "ecc.errors.corrected.aggregate.total"),
			fieldOf(2, "Uncorrectable", "uncorrectable", "ecc.errors.uncorrected.aggregate.total"),
			// Each driver reports its own set of categories (the netlink ones on xe, the others on
			// i915), so only those the device reports are printed.
			optionalFieldOf(2, "Memory Correctable", "memory_correctable", "ras.memory.errors.correctable"),
			optionalFieldOf(2, "Memory Uncorrectable", "memory_uncorrectable", "ras.memory.errors.uncorrectable"),
			optionalFieldOf(2, "PCIe Correctable", "pcie_correctable", "ras.pcie.errors.correctable"),
			optionalFieldOf(2, "PCIe Uncorrectable", "pcie_uncorrectable", "ras.pcie.errors.uncorrectable"),
			optionalFieldOf(2, "SoC Internal Correctable", "soc_internal_correctable", "ras.soc_internal.errors.correctable"),
			optionalFieldOf(2, "SoC Internal Uncorrectable", "soc_internal_uncorrectable", "ras.soc_internal.errors.uncorrectable"),
			optionalFieldOf(2, "Compute Correctable", "compute_correctable", "ras.compute.errors.correctable"),
			optionalFieldOf(2, "Compute Uncorrectable", "compute_uncorrectable", "ras.compute.errors.uncorrectable"),
			optionalFieldOf(2, "Fabric Correctable", "fabric_correctable", "ras.fabric.errors.correctable"),
			optionalFieldOf(2, "Fabric Uncorrectable", "fabric_uncorrectable", "ras.fabric.errors.uncorrectable"),
			optionalFieldOf(2, "Scale Correctable", "scale_correctable", "ras.scale.errors.correctable"),
			optionalFieldOf(2, "Scale Uncorrectable", "scale_uncorrectable", "ras.scale.errors.uncorrectable"),
			optionalFieldOf(2, "Cache Correctable", "cache_correctable", "ras.cache.errors.correctable"),
			optionalFieldOf(2, "Cache Uncorrectable", "cache_uncorrectable", "ras.cache.errors.uncorrectable"),
			optionalFieldOf(2, "Non-Compute Correctable", "non_compute_correctable", "ras.non_compute.errors.correctable"),
			optionalFieldOf(2, "Non-Compute Uncorrectable", "non_compute_uncorrectable", "ras.non_compute.errors.uncorrectable"),
			optionalFieldOf(2, "Resets", "resets", "ras.reset"),
			optionalFieldOf(2, "Programming Errors", "programming_errors", "ras.programming.errors"),
			optionalFieldOf(2, "Driver Errors", "driver_errors", "ras.driver.errors"),
	sectionOf(0, "Temperature", "temperature"),
		fieldOf(1, "GPU Current Temp", "gpu_current_temp", "temperature.gpu"),
		fieldOf(1, "GPU Shutdown Temp", "gpu_shutdown_temp", "temperature.gpu.threshold.shutdown"),
		fieldOf(1, "GPU Slowdown Temp", "gpu_slowdown_temp", "temperature.gpu.threshold.throttle"),
		fieldOf(1, "Memory Current Temp", "memory_current_temp", "temperature.memory"),
		fieldOf(1, "Memory Shutdown Temp", "memory_shutdown_temp", "temperature.memory.threshold.shutdown"),
		fieldOf(1, "Memory Slowdown Temp", "memory_slowdown_temp", "temperature.memory.threshold.throttle"),
	sectionOf(0, "GPU Power Readings", "gpu_power_readings"),
		fieldOf(1, "Average Power Draw", "average_power_draw", "power.draw"),
		optionalFieldOf(1, "Instantaneous Power Draw", "instantaneous_power_draw", "power.draw.instant"),
		optionalFieldOf(1, "GPU Domain Power Draw", "gpu_domain_power_draw", "power.draw.gpu"),
		fieldOf(1, "Current Power Limit", "current_power_limit", "power.limit"),
		optionalFieldOf(1, "Burst Power Limit", "burst_power_limit", "power.limit.burst"),
		optionalFieldOf(1, "Peak Power Limit", "peak_power_limit", "power.limit.peak"),
	sectionOf(0, "Clocks", "clocks"),
		fieldOf(1, "Graphics", "graphics", "clocks.current.graphics"),
		zeroUnknownFieldOf(1, "Memory", "memory", "clocks.current.memory"),
		fieldOf(1, "Media", "media", "clocks.current.media"),
	sectionOf(0, "Applications Clocks", "applications_clocks"),
		fieldOf(1, "Graphics", "graphics", "clocks.requested.graphics"),
	sectionOf(0, "Max Clocks", "max_clocks"),
		fieldOf(1, "Graphics", "graphics", "clocks.max.graphics"),
		fieldOf(1, "Media", "media", "clocks.max.media"),
	sectionOf(0, "Supported Clocks", "supported_clocks"),
		fieldOf(1, "Graphics", "graphics", "clocks.supported.graphics"),
});
// clang-format on

constexpr auto THROTTLE_ROWS = [] {
	std::array<Spec, metrics::clock::THROTTLE_REASONS.size()> rows{};
	std::ranges::transform(metrics::clock::THROTTLE_REASONS, rows.begin(),
						   [](const auto &reason) { return fieldOf(1, reason.label, reason.key, reason.metricName); });
	return rows;
}();

template <std::size_t... N> [[nodiscard]] constexpr auto joinRows(const std::array<Spec, N> &...parts)
{
	std::array<Spec, (N + ...)> rows{};
	auto out = rows.begin();
	((out = std::ranges::copy(parts, out).out), ...);
	return rows;
}

/// The device report, top to bottom. Each row names where its value comes from.
constexpr auto REPORT_LAYOUT = joinRows(LAYOUT_HEAD, THROTTLE_ROWS, LAYOUT_TAIL);

/// Each row is at most one level deeper than the row before it, only a section is followed by a
/// deeper row, and every section has at least one row under it.
[[nodiscard]] constexpr bool wellFormed(std::span<const Spec> rows)
{
	if (rows.empty() || rows.front().depth != 0 || rows.back().kind == SpecKind::Section) {
		return false;
	}
	for (std::size_t i = 1; i < rows.size(); ++i) {
		const Spec &prev = rows[i - 1];
		const bool nested = prev.kind == SpecKind::Section;
		if (nested ? rows[i].depth != prev.depth + 1 : rows[i].depth > prev.depth) {
			return false;
		}
	}
	return true;
}
static_assert(wellFormed(REPORT_LAYOUT));

/// Number of rows from @p rows.front() through its last descendant.
[[nodiscard]] constexpr std::size_t extentOf(std::span<const Spec> rows) noexcept
{
	std::size_t n = 1;
	while (n < rows.size() && rows[n].depth > rows.front().depth) {
		++n;
	}
	return n;
}

// ---------------------------------------------------------------------------------------
// Building the device tree
// ---------------------------------------------------------------------------------------

struct BuildContext
{
	const QuerySelection &selection;
	const MetricValues &values;
	const QueryStaticProps &staticProps;
};

[[nodiscard]] QueryNode leafOf(std::string_view label, std::string_view key, std::optional<std::string> value,
							   std::string_view unit = {})
{
	QueryNode node;
	node.label = label;
	node.key = key;
	node.value = presentOrNull(std::move(value));
	node.unit = unit;
	return node;
}

[[nodiscard]] bool shown(MetricGroup group, const QuerySelection &selection) noexcept
{
	return metrics::hasGroup(group, selection.groups);
}

[[nodiscard]] std::optional<std::string> valueOf(const MetricValues &values, std::string_view name)
{
	const auto it = values.find(name);
	return it == values.end() ? std::nullopt : it->second;
}

[[nodiscard]] std::optional<std::string> pciPartText(SpecKind kind, const std::optional<std::string> &busId)
{
	if (!busId) {
		return std::nullopt;
	}
	const auto parts = bdfParts(*busId);
	if (!parts) {
		return std::nullopt;
	}
	switch (kind) {
	case SpecKind::BdfDomain:
		return xpum::compat::format("0x{:04x}", parts->domain);
	case SpecKind::BdfBus:
		return xpum::compat::format("0x{:02x}", parts->bus);
	case SpecKind::BdfDevice:
		return xpum::compat::format("0x{:02x}", parts->device);
	default:
		return std::nullopt;
	}
}

void buildRows(std::span<const Spec> rows, const BuildContext &ctx, std::vector<QueryNode> &out);

/**
 * @brief Build the node for one spec row, or nothing when the selection hides it or it has nothing to show.
 *
 * @param[in] rows The row followed by its descendants, see extentOf().
 * @param[in] ctx  Selection and collected values.
 */
[[nodiscard]] std::optional<QueryNode> buildRow(std::span<const Spec> rows, const BuildContext &ctx)
{
	const Spec &spec = rows.front();
	switch (spec.kind) {
	case SpecKind::Section: {
		QueryNode section;
		section.label = spec.label;
		section.key = spec.key;
		buildRows(rows.subspan(1), ctx, section.children);
		if (section.children.empty()) {
			return std::nullopt;
		}
		return section;
	}
	case SpecKind::Field: {
		const auto *field = registered(spec.source);
		if (field == nullptr || !shown(field->groups, ctx.selection)) {
			return std::nullopt;
		}
		auto value = valueOf(ctx.values, spec.source);
		if ((spec.negativeIsUnknown && value && value->starts_with('-')) || (spec.zeroIsUnknown && value == "0")) {
			value.reset();
		}
		auto node = leafOf(spec.label, spec.key, std::move(value), field->unit);
		node.hideWhenAbsent = spec.hideWhenAbsent;
		return node;
	}
	case SpecKind::Static: {
		const auto *known = staticSpecOf(spec.source);
		if (known == nullptr || !shown(known->group, ctx.selection) || !ctx.staticProps.has(spec.source)) {
			return std::nullopt;
		}
		return leafOf(spec.label, spec.key, ctx.staticProps.get(spec.source));
	}
	default: {
		const auto *busId = registered(BUS_ID_FIELD);
		if (busId == nullptr || !shown(busId->groups, ctx.selection)) {
			return std::nullopt;
		}
		return leafOf(spec.label, spec.key, pciPartText(spec.kind, valueOf(ctx.values, BUS_ID_FIELD)));
	}
	}
}

/// Build the rows of @p rows that share its first row's depth, each with its descendants, into @p out.
void buildRows(std::span<const Spec> rows, const BuildContext &ctx, std::vector<QueryNode> &out)
{
	while (!rows.empty()) {
		const std::size_t extent = extentOf(rows);
		if (auto node = buildRow(rows.first(extent), ctx)) {
			out.push_back(std::move(*node));
		}
		rows = rows.subspan(extent);
	}
}

// ---------------------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------------------

/// Append "<indent><label><padding>: <value>" with the colon in KEY_COLUMN.
void appendLine(std::string &out, std::size_t depth, std::string_view label, std::string_view value)
{
	const std::size_t labelEnd = depth * INDENT_WIDTH + label.size();
	out.append(depth * INDENT_WIDTH, ' ');
	out.append(label);
	out.append(labelEnd < KEY_COLUMN ? KEY_COLUMN - labelEnd : 1, ' ');
	out.append(": ");
	out.append(value);
	out.push_back('\n');
}

/// Registry getters keep full double precision; the text report shows two decimals.
[[nodiscard]] std::string twoDecimals(const std::string &text)
{
	constexpr std::size_t shownDecimals = 2;
	const auto dot = text.find('.');
	if (dot == std::string::npos || text.size() - dot - 1 <= shownDecimals) {
		return text;
	}
	double number = 0.0;
	const auto *end = text.data() + text.size();
	const auto [stop, error] = std::from_chars(text.data(), end, number);
	if (error != std::errc{} || stop != end) {
		return text;
	}
	return xpum::compat::format("{:.2f}", number);
}

[[nodiscard]] std::string valueText(const QueryNode &node)
{
	if (!node.value) {
		return std::string{NOT_AVAILABLE};
	}
	return node.unit.empty() ? *node.value : twoDecimals(*node.value) + ' ' + std::string{node.unit};
}

[[nodiscard]] nlohmann::ordered_json valueJson(const std::optional<std::string> &value)
{
	return value ? nlohmann::ordered_json(*value) : nlohmann::ordered_json(nullptr);
}

[[nodiscard]] bool equalsIgnoringCase(std::string_view a, std::string_view b) noexcept
{
	return std::ranges::equal(
		a, b, {}, [](char c) { return std::tolower(static_cast<unsigned char>(c)); },
		[](char c) { return std::tolower(static_cast<unsigned char>(c)); });
}

} // namespace

// ---------------------------------------------------------------------------------------
// Public pieces
// ---------------------------------------------------------------------------------------

bool QueryStaticProps::has(std::string_view key) const
{
	return std::ranges::any_of(values, [key](const StaticValue &entry) { return entry.key == key; });
}

std::optional<std::string> QueryStaticProps::get(std::string_view key) const
{
	const auto it = std::ranges::find(values, key, &StaticValue::key);
	return it == values.end() ? std::nullopt : it->value;
}

std::vector<std::string_view> sectionNames()
{
	std::vector<std::string_view> names;
	for (const auto name : metrics::sectionNames()) {
		names.push_back(name);
	}
	names.emplace_back("PIDS");
	return names;
}

std::optional<QuerySelection> parseSelection(std::string_view csv)
{
	const auto names = sectionNames();
	QuerySelection selection{.groups = MetricGroup::NONE, .processes = false};
	bool any = false;
	for (const auto part : csv | std::views::split(',')) {
		const auto token = metrics::detail::trim(std::string_view{part.begin(), part.end()});
		if (token.empty()) {
			continue;
		}
		const auto known =
			std::ranges::any_of(names, [token](std::string_view name) { return equalsIgnoringCase(name, token); });
		if (!known) {
			return std::nullopt;
		}
		any = true;
		if (equalsIgnoringCase(token, "PIDS")) {
			selection.processes = true;
		} else {
			selection.groups = selection.groups | metrics::parseGroupMask(token);
			if (equalsIgnoringCase(token, "ALL")) {
				selection.processes = true;
			}
		}
	}
	if (!any) {
		return std::nullopt;
	}
	return selection;
}

std::optional<BdfParts> bdfParts(std::string_view bdf) noexcept
{
	// "dddd:bb:dd.f", all hex
	constexpr std::size_t length = 12;
	if (bdf.size() != length || bdf[4] != ':' || bdf[7] != ':' || bdf[10] != '.') {
		return std::nullopt;
	}
	const auto field = [bdf](std::size_t pos, std::size_t len) -> std::optional<uint32_t> {
		uint32_t value = 0;
		const auto *first = bdf.data() + pos;
		const auto result = std::from_chars(first, first + len, value, 16);
		if (result.ec != std::errc{} || result.ptr != first + len) {
			return std::nullopt;
		}
		return value;
	};
	const auto domain = field(0, 4);
	const auto bus = field(5, 2);
	const auto device = field(8, 2);
	const auto function = field(11, 1);
	if (!domain || !bus || !device || !function) {
		return std::nullopt;
	}
	return BdfParts{.domain = *domain, .bus = *bus, .device = *device, .function = *function};
}

std::span<const std::string_view> referencedMetricNames()
{
	static const std::vector<std::string_view> NAMES = [] {
		std::vector<std::string_view> names;
		for (const auto &row : REPORT_LAYOUT) {
			const bool readsField = row.kind != SpecKind::Section && row.kind != SpecKind::Static;
			if (readsField && std::ranges::find(names, row.source) == names.end()) {
				names.push_back(row.source);
			}
		}
		return names;
	}();
	return NAMES;
}

std::vector<const metrics::QueryMetric *> selectedMetrics(const QuerySelection &selection)
{
	std::vector<const metrics::QueryMetric *> selected;
	for (const auto name : referencedMetricNames()) {
		if (const auto *field = registered(name); field != nullptr && shown(field->groups, selection)) {
			selected.push_back(field);
		}
	}
	return selected;
}

std::span<const StaticKey> staticKeys()
{
	static const std::vector<StaticKey> KEYS = [] {
		std::vector<StaticKey> keys;
		for (const auto &spec : STATIC_TABLE) {
			keys.push_back({spec.key, spec.group, spec.amcOnly});
		}
		return keys;
	}();
	return KEYS;
}

bool QueryNode::shownInText() const noexcept
{
	if (isLeaf()) {
		return value || !hideWhenAbsent;
	}
	return isList || value || std::ranges::any_of(children, &QueryNode::shownInText);
}

void renderNode(const QueryNode &node, std::size_t depth, std::string &out)
{
	if (!node.shownInText()) {
		return;
	}
	if (node.isLeaf() || node.value) {
		appendLine(out, depth, node.label, valueText(node));
	} else {
		out.append(depth * INDENT_WIDTH, ' ');
		out.append(node.label);
		out.push_back('\n');
	}
	if (node.isList && node.children.empty()) {
		out.append((depth + 1) * INDENT_WIDTH, ' ');
		out.append("None\n");
	}
	for (const auto &child : node.children) {
		renderNode(child, depth + 1, out);
	}
}

std::string renderText(const QueryReport &report)
{
	const auto textOf = [](const std::optional<std::string> &value) {
		return value ? *value : std::string{NOT_AVAILABLE};
	};
	std::string out;
	out.append(LOG_BANNER);
	out.append("\n\n");
	appendLine(out, 0, "Timestamp", report.timestamp);
	appendLine(out, 0, "UMD Version", textOf(report.umdVersion));
	appendLine(out, 0, "KMD Version", textOf(report.kmdVersion));
	appendLine(out, 0, "Level Zero Version", textOf(report.levelZeroVersion));
	out.push_back('\n');
	appendLine(out, 0, "Attached GPUs", std::to_string(report.gpus.size()));
	for (std::size_t i = 0; i < report.gpus.size(); ++i) {
		if (i > 0) {
			out.push_back('\n');
		}
		out.append(report.gpus[i].label);
		out.push_back('\n');
		for (const auto &child : report.gpus[i].children) {
			renderNode(child, 1, out);
		}
	}
	return out;
}

nlohmann::ordered_json nodeToJson(const QueryNode &node)
{
	if (node.isList) {
		auto records = nlohmann::ordered_json::array();
		for (const auto &record : node.children) {
			nlohmann::ordered_json object;
			object[record.key] = valueJson(record.value);
			for (const auto &field : record.children) {
				object[field.key] = nodeToJson(field);
			}
			records.push_back(std::move(object));
		}
		return records;
	}
	if (node.isLeaf()) {
		return valueJson(node.value);
	}
	nlohmann::ordered_json object;
	for (const auto &child : node.children) {
		object[child.key] = nodeToJson(child);
	}
	return object;
}

nlohmann::ordered_json toJson(const QueryReport &report)
{
	nlohmann::ordered_json json;
	json["timestamp"] = report.timestamp;
	json["umd_version"] = valueJson(report.umdVersion);
	json["kmd_version"] = valueJson(report.kmdVersion);
	json["level_zero_version"] = valueJson(report.levelZeroVersion);
	json["attached_gpus"] = report.gpus.size();
	auto gpus = nlohmann::ordered_json::array();
	for (const auto &gpu : report.gpus) {
		nlohmann::ordered_json object;
		object[gpu.key] = valueJson(gpu.value);
		for (const auto &child : gpu.children) {
			object[child.key] = nodeToJson(child);
		}
		gpus.push_back(std::move(object));
	}
	json["gpus"] = std::move(gpus);
	return json;
}

QueryNode processNode(const psInfo &proc)
{
	constexpr std::string_view mebibytes = "MiB";
	QueryNode record = leafOf("Process ID", "process_id", std::to_string(proc.processId));
	// psInfo sizes are in KiB.
	record.children.push_back(
		leafOf("Type", "type", std::string{processTypeFromEngines(proc.engines, proc.memSize, proc.sharedSize)}));
	record.children.push_back(leafOf("Name", "name", proc.commandName));
	record.children.push_back(
		leafOf("Used GPU Memory", "used_gpu_memory", std::to_string(proc.memSize / 1024), mebibytes));
	return record;
}

QueryNode processesSection(std::span<const psInfo> procs)
{
	QueryNode section;
	section.label = "Processes";
	section.key = "processes";
	section.isList = true;
	for (const auto &proc : procs) {
		section.children.push_back(processNode(proc));
	}
	return section;
}

QueryNode buildDeviceNode(std::string_view bdf, const QuerySelection &selection, const MetricValues &metricValues,
						  const QueryStaticProps &staticProps, std::span<const psInfo> procs)
{
	QueryNode device;
	device.label = "GPU " + std::string{bdf};
	device.key = "pci_bus_id";
	device.value = std::string{bdf};

	const BuildContext ctx{selection, metricValues, staticProps};
	buildRows(REPORT_LAYOUT, ctx, device.children);
	if (selection.processes) {
		device.children.push_back(processesSection(procs));
	}
	return device;
}

QueryNode survivabilityNode(std::string_view bdf)
{
	QueryNode device;
	device.label = "GPU " + std::string{bdf};
	device.key = "pci_bus_id";
	device.value = std::string{bdf};
	device.children.push_back(leafOf("Device State", "device_state", std::string{DEVICE_STATE_SURV_MODE}));
	device.children.push_back(leafOf("GPU Recovery Action", "gpu_recovery_action", "Firmware update and GPU reset"));
	return device;
}

} // namespace query

// ---------------------------------------------------------------------------------------
// Collection
// ---------------------------------------------------------------------------------------

namespace query {

namespace {

/**
 * @brief Read the static properties the report shows for a device.
 *
 * Each property is read through the same discovery code the discovery command uses. A
 * property that applies only to devices with an AMC is left out for the others.
 *
 * @param[in] dev       Device to read.
 * @param[in] selection Sections to show; properties of other sections are skipped.
 *
 * @return The properties that apply to the device, each with its value or nothing.
 */
[[nodiscard]] QueryStaticProps gatherStaticProps(devInfo &dev, const QuerySelection &selection)
{
	QueryStaticProps staticProps;
	cmdDiscovery discovery;
	for (const auto &spec : STATIC_TABLE) {
		if (!shown(spec.group, selection) || (spec.amcOnly && !dev.dev->hasAmc())) {
			continue;
		}
		if (spec.compute != nullptr) {
			staticProps.values.push_back({spec.key, spec.compute(dev)});
			continue;
		}
		std::string line;
		const ze_result_t result = (discovery.*spec.getter)(&dev, &line);
		staticProps.values.push_back({spec.key, result == ZE_RESULT_SUCCESS ? std::optional{line} : std::nullopt});
	}
	return staticProps;
}

/// Run each field's getter for one device into @p values; a getter that fails reads as absent.
void evaluate(std::span<const metrics::QueryMetric *const> fields, devInfo &dev, const metrics::MetricCache &cache,
			  MetricValues &values)
{
	for (const auto *field : fields) {
		metrics::MetricValue out;
		values[field->name] = field->getter(dev, out, cache) == ZE_RESULT_SUCCESS ? std::optional{out} : std::nullopt;
	}
}

/// Fields grouped by when they are read: from the firmware, at one point in time, or over the window.
struct FieldSplit
{
	std::vector<const metrics::QueryMetric *> firmware;
	std::vector<const metrics::QueryMetric *> instant;
	std::vector<const metrics::QueryMetric *> windowed;
};

[[nodiscard]] FieldSplit splitByWindow(std::span<const metrics::QueryMetric *const> fields)
{
	FieldSplit split;
	for (const auto *field : fields) {
		if (field->readsFirmware) {
			split.firmware.push_back(field);
		} else if (field->inputs == metrics::MetricInput::NONE) {
			split.instant.push_back(field);
		} else {
			split.windowed.push_back(field);
		}
	}
	return split;
}

/**
 * @brief Reads each firmware field of one device on a thread of its own and waits for them all.
 *
 * Each of these reads is a separate firmware request that can take hundreds of milliseconds,
 * so reading them one after another could outlast the sampling window.
 */
[[nodiscard]] MetricValues readFirmwareFields(std::span<const metrics::QueryMetric *const> fields, devInfo &dev,
											  const metrics::MetricCache &cache)
{
	std::vector<std::future<std::optional<std::string>>> reads;
	reads.reserve(fields.size());
	for (const auto *field : fields) {
		reads.push_back(std::async(std::launch::async, [field, &dev, &cache]() -> std::optional<std::string> {
			metrics::MetricValue out;
			return field->getter(dev, out, cache) == ZE_RESULT_SUCCESS ? std::optional{out} : std::nullopt;
		}));
	}
	MetricValues values;
	for (std::size_t i = 0; i < fields.size(); ++i) {
		values[fields[i]->name] = reads[i].get();
	}
	return values;
}

[[nodiscard]] std::string nowIso8601()
{
	return xpum::compat::format("{:%Y-%m-%dT%H:%M:%S}Z",
								std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
}

/**
 * Samples the devices and renders one report per call.
 *
 * The first frame measures over one sampling window and reads the static properties. Later
 * frames measure over the time since the previous frame and reuse the static properties,
 * so a loop does not pay the slow firmware queries again.
 */
class QuerySession
{
public:
	QuerySession(arg_struct *cli, std::vector<devInfo> found, std::vector<devInfo> survivabilityFound,
				 const QueryOptions &opts)
		: args(cli), devices(std::move(found)), survivability(std::move(survivabilityFound)), options(opts),
		  fields(selectedMetrics(opts.selection)), split(splitByWindow(fields)), inputs(metrics::inputsOf(fields)),
		  caches(devices.size()), staticProps(devices.size()), firmwareValues(devices.size())
	{}

	[[nodiscard]] RenderedFrame renderFrame();

private:
	[[nodiscard]] QueryNode sampleDevice(std::size_t index);
	[[nodiscard]] std::optional<std::string> headerValue(std::string_view fieldName);

	arg_struct *args;
	std::vector<devInfo> devices;
	std::vector<devInfo> survivability;
	const QueryOptions &options;
	std::vector<const metrics::QueryMetric *> fields;
	FieldSplit split;
	metrics::MetricInput inputs;
	std::vector<metrics::MetricCache> caches;
	std::vector<QueryStaticProps> staticProps;
	/// Firmware fields of each device, read on the first frame only: they cannot change while the
	/// device runs, and a read that fails would otherwise be retried, as slowly, on every frame.
	std::vector<MetricValues> firmwareValues;
	bool firstFrame = true;
};

QueryNode QuerySession::sampleDevice(std::size_t index)
{
	devInfo &dev = devices[index];
	metrics::MetricCache &cache = caches[index];
	const bool windowed = inputs != metrics::MetricInput::NONE;
	const bool first = firstFrame;

	// A cache without a window is kept from frame to frame; what it read on first use must still be new.
	cache.rasRead.reset();
	cache.throttleRead.reset();
	auto windowEnd = std::chrono::steady_clock::time_point{};
	if (windowed) {
		if (first) {
			cache = metrics::populateMetricCacheBegin(dev, inputs);
			windowEnd = std::chrono::steady_clock::now() + metrics::detail::SAMPLE_WINDOW;
		} else {
			cache = metrics::populateMetricCacheContinuous(dev, cache, inputs);
		}
	}

	// Work that does not depend on the window runs while it elapses. As root on some platforms
	// each firmware read takes most of the window, so they run on threads of their own.
	// On a card with an AMC the serial number and the board part number are both read from the
	// AMC, which does not serialize requests, so the static properties follow the firmware fields
	// in the same task rather than run beside them.
	std::future<MetricValues> firmwareRead;
	if (first) {
		firmwareRead = std::async(std::launch::async, [this, index, &dev, &cache] {
			MetricValues read = readFirmwareFields(split.firmware, dev, cache);
			staticProps[index] = gatherStaticProps(dev, options.selection);
			return read;
		});
	}
	MetricValues values;
	values.reserve(fields.size());
	evaluate(split.instant, dev, cache, values);
	std::vector<psInfo> procs;
	if (options.selection.processes) {
		if (const auto result = cmdPs::getProcessList(&dev, procs); isFatalPsError(result)) {
			DBG("Could not list processes on device {}: 0x{:X}\n", dev.index, static_cast<uint32_t>(result));
			procs.clear();
		}
	}

	// Joined before the end of the window is sampled into the cache the firmware reads were given.
	if (first) {
		firmwareValues[index] = firmwareRead.get();
	}
	values.insert(firmwareValues[index].begin(), firmwareValues[index].end());
	if (windowed && first) {
		std::this_thread::sleep_until(windowEnd);
		metrics::populateMetricCacheEnd(dev, cache);
	}
	evaluate(split.windowed, dev, cache, values);
	return buildDeviceNode(devPciAddr(dev), options.selection, values, staticProps[index], procs);
}

std::optional<std::string> QuerySession::headerValue(std::string_view fieldName)
{
	const auto *field = registered(fieldName);
	if (field == nullptr || devices.empty()) {
		return std::nullopt;
	}
	metrics::MetricValue out;
	const metrics::MetricCache cache;
	if (field->getter(devices.front(), out, cache) != ZE_RESULT_SUCCESS) {
		return std::nullopt;
	}
	return presentOrNull(std::move(out));
}

RenderedFrame QuerySession::renderFrame()
{
	// The timestamp marks when sampling began, not when it ended.
	QueryReport report;
	report.timestamp = nowIso8601();

	std::vector<std::future<QueryNode>> tasks;
	tasks.reserve(devices.size());
	for (std::size_t i = 0; i < devices.size(); ++i) {
		tasks.push_back(std::async(std::launch::async, [this, i] { return sampleDevice(i); }));
	}
	for (auto &task : tasks) {
		report.gpus.push_back(task.get());
	}
	for (auto &dev : survivability) {
		report.gpus.push_back(survivabilityNode(devPciAddr(dev)));
	}
	firstFrame = false;

	report.umdVersion = headerValue("driver_version");
	report.kmdVersion = headerValue("kernel_driver_version");
	report.levelZeroVersion = presentOrNull(args->sm.getLoaderVersion());

	return RenderedFrame{ZE_RESULT_SUCCESS, options.json ? toJson(report).dump(4) + "\n" : renderText(report)};
}

} // namespace

} // namespace query

// ---------------------------------------------------------------------------------------
// The command
// ---------------------------------------------------------------------------------------

namespace {

constexpr auto SINGLE_SHOT_INTERVAL = std::chrono::milliseconds{0};
constexpr auto DEFAULT_LOOP_INTERVAL = std::chrono::seconds{1};
constexpr int OPTION_TEXT_WIDTH = 72;
constexpr std::size_t ERROR_LIST_WIDTH = 68;

/// The normal and the survivability GPUs a --device value names.
struct SelectedDevices
{
	std::vector<devInfo> normal;
	std::vector<devInfo> survivability;
};

/**
 * @brief Resolves a --device value against both the normal and the survivability GPUs.
 *
 * Each comma-separated selector is looked up among the survivability GPUs first, so one that
 * names such a GPU is not also reported as an unknown normal device. A selector that names
 * no GPU is reported and skipped.
 *
 * @param[in] sm   The driver that enumerated the GPUs.
 * @param[in] spec Device indices or PCI addresses, comma-separated; empty selects every GPU.
 *
 * @return The selected GPUs of each kind, without duplicates, in the order they were named.
 */
[[nodiscard]] SelectedDevices selectDevices(driver &sm, std::string_view spec)
{
	SelectedDevices selected;
	if (spec.empty()) {
		std::ignore = sm.findDevice("", &selected.normal);
		sm.findSurvDevice("", &selected.survivability);
		return selected;
	}
	for (const auto part : spec | std::views::split(',')) {
		const std::string token{part.begin(), part.end()};
		if (token.empty()) {
			continue;
		}
		const std::size_t before = selected.survivability.size();
		sm.findSurvDevice(token.c_str(), &selected.survivability);
		if (selected.survivability.size() == before) {
			// A selector that matches nothing is logged by findDevice.
			std::ignore = sm.findDevice(token.c_str(), &selected.normal);
		}
	}
	xpum::detail::deduplicateByIndex(selected.normal);
	xpum::detail::deduplicateByIndex(selected.survivability);
	return selected;
}

/// `query` with no arguments prints the report; only `query help` is a help request.
[[nodiscard]] bool printsHelpOnly(std::span<char *const> cmdArgs)
{
	return cmdArgs.size() == 1 && std::string_view{cmdArgs.front()} == "help";
}

/// Join section names with commas and break the text into lines of at most @p width characters.
[[nodiscard]] std::vector<std::string> wrapSectionNames(std::size_t width)
{
	const auto names = query::sectionNames();
	std::vector<std::string> lines;
	std::string current;
	for (std::size_t i = 0; i < names.size(); ++i) {
		const std::string piece = std::string{names[i]} + (i + 1 < names.size() ? "," : "");
		if (!current.empty() && current.size() + 1 + piece.size() > width) {
			lines.push_back(std::move(current));
			current.clear();
		}
		current += current.empty() ? piece : ' ' + piece;
	}
	if (!current.empty()) {
		lines.push_back(std::move(current));
	}
	return lines;
}

// clang-format off
constexpr auto OPTIONS = std::to_array<std::pair<std::string_view, std::string_view>>({
	{"-h,--help",               "Print this help message and exit"},
	{"-j,--json",               "Print the report as JSON"},
	{"-d,--device,--id <spec>", "Device index or PCI BDF address, comma-separated (default: all)"},
	{"--display <sections>",    "Comma-separated sections to print (default: all but SUPPORTED_CLOCKS):"},
	{"--loop <sec>",            "Print the report every <sec> seconds"},
	{"--loop-ms <ms>",          "Print the report every <ms> milliseconds (overrides --loop)"},
	{"--count <n>",             "Number of reports (default: unlimited with --loop)"},
	{"-f,--file <path>",        "Write the report to <path> instead of stdout"},
});
// clang-format on

/// Append the options as an aligned two-column list whose descriptions wrap at a space.
void appendOptions(std::vector<helpCmd> &helpList)
{
	TableBuilder table;
	table.setMaxCellWidth(OPTION_TEXT_WIDTH + 2).enableWordWrap().addColumn("").addColumn("").setColumnWrap(0, false);
	for (const auto &[option, text] : OPTIONS) {
		if (option.starts_with("--display")) {
			std::string withNames{text};
			const auto names = query::sectionNames();
			for (std::size_t i = 0; i < names.size(); ++i) {
				withNames += xpum::compat::format(" {}{}", names[i], i + 1 < names.size() ? "," : "");
			}
			table.addRow(option, withNames);
		} else {
			table.addRow(option, text);
		}
	}
	// The borderless table frames each line with a space and pads it; the help list indents instead.
	for (const auto part : std::views::split(table.asTable(), '\n')) {
		std::string_view line{part.begin(), part.end()};
		const auto last = line.find_last_not_of(' ');
		if (last == std::string_view::npos) {
			continue;
		}
		line = line.substr(0, last + 1);
		line.remove_prefix(std::min(line.find_first_not_of(' '), std::size_t{2}));
		helpList.emplace_back(HEADING, "%.*s", static_cast<int>(line.size()), line.data());
	}
}

/**
 * @brief The help text of an option, from the same table `query --help` prints.
 *
 * @param[in] option The option as the table names it, without its argument, e.g. "--display".
 *
 * @retval text The option's help text; empty for an option the table does not list.
 */
[[nodiscard]] std::string optionHelp(std::string_view option)
{
	const auto it = std::ranges::find_if(OPTIONS, [option](const auto &entry) {
		const std::string_view name = entry.first;
		return name == option || (name.starts_with(option) && name[option.size()] == ' ');
	});
	return it == OPTIONS.end() ? std::string{} : std::string{it->second};
}

} // namespace

void cmdQuery::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.emplace_back(TITLE, "Print a report of the main attributes of each GPU");
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Usage: %s query [Options]", progName.c_str());
	helpList.emplace_back(HEADING, "%s query --display MEMORY,ECC --id 0", progName.c_str());
	helpList.emplace_back(HEADING, "%s query --json", progName.c_str());
	helpList.emplace_back(HEADING, "%s query --loop 5 --count 3 --file report.txt", progName.c_str());
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Options:");
	appendOptions(helpList);
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Notes:");
	helpList.emplace_back(HEADING, "A value the device cannot supply is printed as N/A.");
	helpList.emplace_back(HEADING, "Static detail is in 'discovery'; single metrics are in --query-gpu and 'dump'.");
	helpList.emplace_back(HEADING, "Engine utilization needs CAP_PERFMON; without it the values read N/A.");
	helpList.emplace_back(BLANK);

	printHelp(helpList, helpType);
}

/**
 * @brief `query help` only prints help; any other form reads devices.
 *
 * @param[in] cmdArgs argv tokens after the subcommand name.
 *
 * @retval DriverMode::None `query help`.
 * @retval DriverMode::Full Otherwise.
 */
DriverMode cmdQuery::driverMode(std::span<char *const> cmdArgs) const
{
	return printsHelpOnly(cmdArgs) ? DriverMode::None : DriverMode::Full;
}

std::variant<QueryOptions, int> cmdQuery::parseOptions(arg_struct *args)
{
	QueryOptions opts;
	std::optional<std::string> display;
	int loopSeconds = 0;
	int loopMilliseconds = 0;

	CLI::App sub{"Print a report of the main attributes of each GPU", "query"};
	sub.set_help_flag("-h,--help", optionHelp("-h,--help"));
	sub.add_flag("-j,--json", opts.json, optionHelp("-j,--json"));
	sub.add_option("-d,--device,--id", opts.deviceSpec, optionHelp("-d,--device,--id"));
	sub.add_option("--display", display, optionHelp("--display"));
	sub.add_option("--loop", loopSeconds, optionHelp("--loop"))->check(CLI::Range(1, std::numeric_limits<int>::max()));
	sub.add_option("--loop-ms", loopMilliseconds, optionHelp("--loop-ms"))
		->check(CLI::Range(1, std::numeric_limits<int>::max()));
	sub.add_option("--count", opts.count, optionHelp("--count"))->check(CLI::Range(1, std::numeric_limits<int>::max()));
	sub.add_option("-f,--file", opts.file, optionHelp("-f,--file"));

	try {
		sub.parse(args->argc - 1, args->argv + 1);
	} catch (const CLI::CallForHelp &) {
		cmdQuery{}.help();
		return static_cast<int>(ZE_RESULT_SUCCESS);
	} catch (const CLI::ParseError &e) {
		ERR("{}\n", e.what());
		ERR("Run with --help for more information.\n");
		return static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT);
	}

	if (opts.deviceSpec == "-1") {
		opts.deviceSpec.clear();
	}
	if (display) {
		const auto selection = query::parseSelection(*display);
		if (!selection) {
			ERR("No valid display section matched: '{}'\n", *display);
			ERR("Valid sections are:\n");
			for (const auto &line : wrapSectionNames(ERROR_LIST_WIDTH)) {
				ERR("  {}\n", line);
			}
			return static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT);
		}
		opts.selection = *selection;
	}

	if (loopMilliseconds > 0) {
		opts.loop = std::chrono::milliseconds{loopMilliseconds};
	} else if (loopSeconds > 0) {
		opts.loop = std::chrono::seconds{loopSeconds};
	}
	if (opts.count > 0 && opts.loop == SINGLE_SHOT_INTERVAL) {
		opts.loop = DEFAULT_LOOP_INTERVAL;
	}
	if (opts.count == 1) {
		// One report needs no loop, and a loop would draw it on the alternate screen and erase it on exit.
		opts.loop = SINGLE_SHOT_INTERVAL;
	}
	return opts;
}

int cmdQuery::run(arg_struct *args)
{
	TRACING();

	if (printsHelpOnly(subcommandArgs(*args))) {
		help();
		return static_cast<int>(ZE_RESULT_SUCCESS);
	}

	auto parsed = parseOptions(args);
	if (const int *code = std::get_if<int>(&parsed)) {
		return *code;
	}
	const auto opts = std::get<QueryOptions>(std::move(parsed));

	auto [devices, survivability] = selectDevices(args->sm, opts.deviceSpec);
	if (devices.empty() && survivability.empty()) {
		if (!opts.deviceSpec.empty()) {
			ERR("Error: Device handle not found for device ID '{}'\n", opts.deviceSpec);
			return static_cast<int>(ZE_RESULT_ERROR_INVALID_ARGUMENT);
		}
		ERR("Error: No devices found.\n");
		return static_cast<int>(ZE_RESULT_ERROR_NOT_AVAILABLE);
	}

	// Only the reports go to the file; diagnostics stay on the terminal, so a JSON report stays valid.
	std::ofstream file;
	FrameLoopOptions loop{.interval = opts.loop,
						  .count = opts.count,
						  .tty = stdoutIsTerminal() && !opts.file,
						  .redraw = Redraw::ChangedLines};
	// A value the device cannot supply already reads N/A in the report, so the error behind it
	// would only repeat that, once per device.
	loop.quietRender = true;
	if (opts.file) {
		file.open(*opts.file, std::ios::out | std::ios::trunc);
		if (!file.is_open()) {
			ERR("Failed to open file '{}'\n", *opts.file);
			return static_cast<int>(ZE_RESULT_ERROR_UNKNOWN);
		}
		// A write that fails (e.g. a full disk) ends the loop instead of sampling on to the end.
		loop.write = [&file](std::string_view text) { return static_cast<bool>(file << text << std::flush); };
	}

	query::QuerySession session{args, std::move(devices), std::move(survivability), opts};
	const int rc = runFrameLoop([&session] { return session.renderFrame(); }, loop);
	if (opts.file && !file) {
		ERR("Failed to write file '{}'\n", *opts.file);
		return rc != 0 ? rc : static_cast<int>(ZE_RESULT_ERROR_UNKNOWN);
	}
	return rc;
}
