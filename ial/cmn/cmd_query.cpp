/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "cmd_query.h"
#include "cmd_discovery.h"
#include "debug.h"
#include "driver_util.h"
#include "frame_loop.h"
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

/** One static property: where its text comes from and which section shows it. */
struct StaticSpec
{
	std::string_view key;
	MetricGroup group;
	discoverySubCmdFunc getter; ///< nullptr for a property computed from the device itself.
	bool amcOnly;				///< Only devices with an AMC have it.
};

// clang-format off
constexpr auto STATIC_TABLE = std::to_array<StaticSpec>({
	{"device_type",                    MetricGroup::IDENTITY, &cmdDiscovery::deviceType,                   false},
	{"sku_type",                       MetricGroup::IDENTITY, &cmdDiscovery::skuType,                      false},
	{"stepping",                       MetricGroup::IDENTITY, &cmdDiscovery::stepping,                     false},
	{"device_state",                   MetricGroup::IDENTITY, nullptr,                                     false},
	{"vendor_name",                    MetricGroup::IDENTITY, &cmdDiscovery::vendorName,                   false},
	{"part_number",                    MetricGroup::IDENTITY, &cmdDiscovery::partNumber,                   true},
	{"drm_device",                     MetricGroup::IDENTITY, &cmdDiscovery::drmDevice,                    false},
	{"kernel_version",                 MetricGroup::IDENTITY, &cmdDiscovery::kernelVersion,                false},
	{"gfx_firmware_status",            MetricGroup::IDENTITY, &cmdDiscovery::gfxFirmwareStatus,            false},
	{"gfx_data_firmware_version",      MetricGroup::IDENTITY, &cmdDiscovery::gfxDataFirmwareVersion,       false},
	{"amc_firmware_version",           MetricGroup::IDENTITY, &cmdDiscovery::amcFirmwareVersion,           true},
	{"gfx_pscbin_firmware_version",    MetricGroup::IDENTITY, &cmdDiscovery::gfxPscBinFirmwareVersion,     true},
	{"oprom_code_firmware_version",    MetricGroup::IDENTITY, &cmdDiscovery::opromCodeFirmwareVersion,     true},
	{"oprom_data_firmware_version",    MetricGroup::IDENTITY, &cmdDiscovery::opromDataFirmwareVersion,     true},
	{"number_of_tiles",                MetricGroup::IDENTITY, &cmdDiscovery::numberOfTiles,                false},
	{"number_of_slices",               MetricGroup::IDENTITY, &cmdDiscovery::numberOfSlices,               false},
	{"number_of_sub_slices_per_slice", MetricGroup::IDENTITY, &cmdDiscovery::numberOfSubslicesPerSlice,    false},
	{"number_of_eus_per_sub_slice",    MetricGroup::IDENTITY, &cmdDiscovery::numberOfEUsPerSubslice,       false},
	{"number_of_eus",                  MetricGroup::IDENTITY, &cmdDiscovery::eus,                          false},
	{"number_of_threads_per_eu",       MetricGroup::IDENTITY, &cmdDiscovery::numberOfThreadsPerEU,         false},
	{"physical_eu_simd_width",         MetricGroup::IDENTITY, &cmdDiscovery::physicalEUSimdWidth,          false},
	{"number_of_media_engines",        MetricGroup::IDENTITY, &cmdDiscovery::mediaEngines,                 false},
	{"number_of_media_enh_engines",    MetricGroup::IDENTITY, &cmdDiscovery::mediaEnhancementEngines,      false},
	{"max_hardware_contexts",          MetricGroup::IDENTITY, &cmdDiscovery::maxHardwareContexts,          false},
	{"max_command_queue_priority",     MetricGroup::IDENTITY, &cmdDiscovery::maxCommandQueuePriority,      false},
	{"pci_vendor_id",                  MetricGroup::PCI,      &cmdDiscovery::pciVendorID,                  false},
	{"pci_slot",                       MetricGroup::PCI,      &cmdDiscovery::pciSlot,                      false},
	{"pcie_max_bandwidth",             MetricGroup::PCI,      &cmdDiscovery::pcieMaxBandwidth,             false},
	{"memory_type",                    MetricGroup::MEMORY,   &cmdDiscovery::memoryType,                   false},
	{"memory_vendor",                  MetricGroup::MEMORY,   &cmdDiscovery::memoryVendor,                 false},
	{"number_of_memory_channels",      MetricGroup::MEMORY,   &cmdDiscovery::memoryChannels,               false},
	{"memory_bus_width",               MetricGroup::MEMORY,   &cmdDiscovery::memoryBusWidth,               false},
	{"max_mem_alloc_size",             MetricGroup::MEMORY,   &cmdDiscovery::maxMemAllocSize,              false},
	{"core_clock_rate",                MetricGroup::CLOCK,    &cmdDiscovery::coreClockRate,                false},
	{"tdp",                            MetricGroup::POWER,    &cmdDiscovery::tdp,                          true},
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

/// An empty text is a value the platform could not supply, so it reads as absent.
[[nodiscard]] std::optional<std::string> presentOrNull(std::optional<std::string> value)
{
	if (value && value->empty()) {
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
	BdfFunction,
};

/** Describes one row or section of the device report. */
struct Spec
{
	SpecKind kind;
	std::string_view label;
	std::string_view key;
	std::string_view source; ///< Registry field name, or static property key.
	std::vector<Spec> children;
	bool negativeIsUnknown = false; ///< A negative reading is the driver's way of saying "unknown".
};

constexpr std::string_view BUS_ID_FIELD = "pci.bus_id";

/// Discovery reports the largest single allocation in bytes; the report shows it in MiB like the other sizes.
constexpr std::string_view ALLOCATION_SIZE_KEY = "max_mem_alloc_size";
constexpr std::string_view MEBIBYTES = "MiB";

[[nodiscard]] Spec sectionOf(std::string_view label, std::string_view key, std::vector<Spec> children)
{
	return Spec{SpecKind::Section, label, key, {}, std::move(children)};
}

[[nodiscard]] Spec fieldOf(std::string_view label, std::string_view key, std::string_view name)
{
	return Spec{SpecKind::Field, label, key, name, {}};
}

/// A field whose driver reports -1 when it does not know the value.
[[nodiscard]] Spec linkFieldOf(std::string_view label, std::string_view key, std::string_view name)
{
	Spec spec = fieldOf(label, key, name);
	spec.negativeIsUnknown = true;
	return spec;
}

[[nodiscard]] Spec staticOf(std::string_view label, std::string_view key, std::string_view staticKey)
{
	return Spec{SpecKind::Static, label, key, staticKey, {}};
}

[[nodiscard]] Spec pciPartOf(SpecKind kind, std::string_view label, std::string_view key)
{
	return Spec{kind, label, key, BUS_ID_FIELD, {}};
}

/// The device report, top to bottom. Each row names where its value comes from.
[[nodiscard]] const std::vector<Spec> &reportLayout()
{
	static const std::vector<Spec> LAYOUT = [] {
		std::vector<Spec> rows;
		rows.push_back(fieldOf("Product Name", "product_name", "name"));
		rows.push_back(fieldOf("Device Index", "device_index", "index"));
		rows.push_back(staticOf("Device Type", "device_type", "device_type"));
		rows.push_back(staticOf("SKU Type", "sku_type", "sku_type"));
		rows.push_back(staticOf("Stepping", "stepping", "stepping"));
		rows.push_back(staticOf("Device State", "device_state", "device_state"));
		rows.push_back(fieldOf("Serial Number", "serial_number", "serial"));
		rows.push_back(fieldOf("GPU UUID", "gpu_uuid", "uuid"));
		rows.push_back(staticOf("Vendor Name", "vendor_name", "vendor_name"));
		rows.push_back(staticOf("Part Number", "part_number", "part_number"));
		rows.push_back(staticOf("DRM Device", "drm_device", "drm_device"));
		rows.push_back(sectionOf("Driver", "driver",
								 {
									 fieldOf("UMD Version", "umd_version", "driver_version"),
									 fieldOf("KMD Version", "kmd_version", "kernel_driver_version"),
									 staticOf("Kernel Version", "kernel_version", "kernel_version"),
								 }));
		rows.push_back(sectionOf(
			"Firmware", "firmware",
			{
				fieldOf("GFX Firmware Version", "gfx_firmware_version", "vbios_version"),
				staticOf("GFX Firmware Status", "gfx_firmware_status", "gfx_firmware_status"),
				staticOf("GFX Data Firmware Version", "gfx_data_firmware_version", "gfx_data_firmware_version"),
				staticOf("AMC Firmware Version", "amc_firmware_version", "amc_firmware_version"),
				staticOf("GFX PSCBIN Firmware Version", "gfx_pscbin_firmware_version", "gfx_pscbin_firmware_version"),
				staticOf("OPROM Code Firmware Version", "oprom_code_firmware_version", "oprom_code_firmware_version"),
				staticOf("OPROM Data Firmware Version", "oprom_data_firmware_version", "oprom_data_firmware_version"),
			}));
		rows.push_back(sectionOf(
			"Compute Resources", "compute_resources",
			{
				staticOf("Number of Tiles", "number_of_tiles", "number_of_tiles"),
				staticOf("Number of Slices", "number_of_slices", "number_of_slices"),
				staticOf("Number of Sub-slices per Slice", "number_of_sub_slices_per_slice",
						 "number_of_sub_slices_per_slice"),
				staticOf("Number of EUs per Sub-slice", "number_of_eus_per_sub_slice", "number_of_eus_per_sub_slice"),
				staticOf("Number of EUs", "number_of_eus", "number_of_eus"),
				staticOf("Number of Threads per EU", "number_of_threads_per_eu", "number_of_threads_per_eu"),
				staticOf("Physical EU SIMD Width", "physical_eu_simd_width", "physical_eu_simd_width"),
				staticOf("Number of Media Engines", "number_of_media_engines", "number_of_media_engines"),
				staticOf("Number of Media Enhancement Engines", "number_of_media_enh_engines",
						 "number_of_media_enh_engines"),
				staticOf("Max Hardware Contexts", "max_hardware_contexts", "max_hardware_contexts"),
				staticOf("Max Command Queue Priority", "max_command_queue_priority", "max_command_queue_priority"),
			}));
		rows.push_back(
			sectionOf("PCI", "pci",
					  {
						  pciPartOf(SpecKind::BdfBus, "Bus", "bus"),
						  pciPartOf(SpecKind::BdfDevice, "Device", "device"),
						  pciPartOf(SpecKind::BdfDomain, "Domain", "domain"),
						  pciPartOf(SpecKind::BdfFunction, "Function", "function"),
						  fieldOf("Bus Id", "bus_id", BUS_ID_FIELD),
						  fieldOf("Device Id", "device_id", "pci.device_id"),
						  fieldOf("Sub System Id", "sub_system_id", "pci.sub_device_id"),
						  staticOf("Vendor Id", "vendor_id", "pci_vendor_id"),
						  staticOf("PCI Slot", "pci_slot", "pci_slot"),
						  sectionOf("GPU Link Info", "gpu_link_info",
									{
										sectionOf("PCIe Generation", "pcie_generation",
												  {
													  linkFieldOf("Max", "max", "pcie.link.gen.max"),
													  linkFieldOf("Current", "current", "pcie.link.gen.current"),
												  }),
										sectionOf("Link Width", "link_width",
												  {
													  linkFieldOf("Max", "max", "pcie.link.width.max"),
													  linkFieldOf("Current", "current", "pcie.link.width.current"),
												  }),
									}),
						  staticOf("Max Bandwidth", "max_bandwidth", "pcie_max_bandwidth"),
						  fieldOf("Replays Since Reset", "replays_since_reset", "pcie.replay.counter"),
						  fieldOf("Tx Throughput", "tx_throughput", "pcie.tx.throughput"),
						  fieldOf("Rx Throughput", "rx_throughput", "pcie.rx.throughput"),
					  }));
		rows.push_back(fieldOf("Fan Speed", "fan_speed", "fan.speed"));
		rows.push_back(sectionOf(
			"Memory Usage", "memory_usage",
			{
				fieldOf("Total", "total", "memory.total"),
				fieldOf("Used", "used", "memory.used"),
				fieldOf("Free", "free", "memory.free"),
				fieldOf("Memory Utilization", "memory_utilization", "utilization.memory"),
				fieldOf("Read Bandwidth", "read_bandwidth", "memory.read.bandwidth"),
				fieldOf("Write Bandwidth", "write_bandwidth", "memory.write.bandwidth"),
				fieldOf("Bandwidth Utilization", "bandwidth_utilization", "memory.bandwidth.utilization"),
				staticOf("Memory Type", "memory_type", "memory_type"),
				staticOf("Memory Vendor", "memory_vendor", "memory_vendor"),
				staticOf("Number of Memory Channels", "number_of_memory_channels", "number_of_memory_channels"),
				staticOf("Memory Bus Width", "memory_bus_width", "memory_bus_width"),
				staticOf("Max Memory Alloc Size", "max_memory_alloc_size", "max_mem_alloc_size"),
			}));
		rows.push_back(sectionOf("Utilization", "utilization",
								 {
									 fieldOf("GPU", "gpu", "utilization.gpu"),
									 fieldOf("Compute", "compute", "utilization.compute"),
									 fieldOf("Render", "render", "utilization.render"),
									 fieldOf("Media", "media", "utilization.media"),
									 fieldOf("Copy", "copy", "utilization.copy"),
									 sectionOf("EU Array", "eu_array",
											   {
												   fieldOf("Active", "active", "eu.active"),
												   fieldOf("Stall", "stall", "eu.stall"),
												   fieldOf("Idle", "idle", "eu.idle"),
											   }),
								 }));
		rows.push_back(sectionOf("ECC Mode", "ecc_mode",
								 {
									 fieldOf("Current", "current", "ecc.mode.current"),
								 }));
		rows.push_back(sectionOf(
			"ECC Errors", "ecc_errors",
			{
				sectionOf("Aggregate", "aggregate",
						  {
							  fieldOf("Correctable", "correctable", "ecc.errors.corrected.aggregate.total"),
							  fieldOf("Uncorrectable", "uncorrectable", "ecc.errors.uncorrected.aggregate.total"),
							  fieldOf("Total", "total", "ecc.errors.aggregate.total"),
						  }),
				sectionOf(
					"RAS Counters", "ras_counters",
					{
						fieldOf("Reset", "reset", "ras.reset"),
						fieldOf("Programming Errors", "programming_errors", "ras.programming.errors"),
						fieldOf("Driver Errors", "driver_errors", "ras.driver.errors"),
						sectionOf("Cache Errors", "cache_errors",
								  {
									  fieldOf("Correctable", "correctable", "ras.cache.errors.correctable"),
									  fieldOf("Uncorrectable", "uncorrectable", "ras.cache.errors.uncorrectable"),
								  }),
						sectionOf("Non-Compute Errors", "non_compute_errors",
								  {
									  fieldOf("Correctable", "correctable", "ras.non_compute.errors.correctable"),
									  fieldOf("Uncorrectable", "uncorrectable", "ras.non_compute.errors.uncorrectable"),
									  fieldOf("Total", "total", "ras.non_compute.errors.total"),
								  }),
					}),
			}));
		rows.push_back(sectionOf("Temperature", "temperature",
								 {
									 fieldOf("GPU Current Temp", "gpu_current_temp", "temperature.gpu"),
									 fieldOf("Memory Current Temp", "memory_current_temp", "temperature.memory"),
									 fieldOf("Composite Temp", "composite_temp", "temperature.composite"),
								 }));
		rows.push_back(sectionOf("Power Readings", "power_readings",
								 {
									 fieldOf("Power Draw", "power_draw", "power.draw"),
									 fieldOf("GPU Power Draw", "gpu_power_draw", "power.draw.gpu"),
									 fieldOf("Current Power Limit", "current_power_limit", "power.limit"),
									 fieldOf("Max Power Limit", "max_power_limit", "power.max_limit"),
									 fieldOf("Energy Consumed", "energy_consumed", "energy.consumed"),
									 fieldOf("GPU Energy Consumed", "gpu_energy_consumed", "energy.consumed.gpu"),
									 staticOf("Thermal Design Power", "thermal_design_power", "tdp"),
								 }));
		rows.push_back(sectionOf("Clocks", "clocks",
								 {
									 fieldOf("Graphics", "graphics", "clocks.current.graphics"),
									 fieldOf("Media", "media", "clocks.current.media"),
									 fieldOf("Memory", "memory", "clocks.current.memory"),
									 fieldOf("Memory Voltage", "memory_voltage", "clocks.current.memory.voltage"),
									 staticOf("Core Clock Rate", "core_clock_rate", "core_clock_rate"),
									 fieldOf("Clocks Event Reasons", "clocks_event_reasons", "clocks.throttle.reason"),
								 }));
		rows.push_back(sectionOf("Max Clocks", "max_clocks",
								 {
									 fieldOf("Graphics", "graphics", "clocks.max.graphics"),
									 fieldOf("Media", "media", "clocks.max.media"),
								 }));
		return rows;
	}();
	return LAYOUT;
}

void collectFieldNames(const std::vector<Spec> &rows, std::vector<std::string_view> &names)
{
	for (const auto &row : rows) {
		switch (row.kind) {
		case SpecKind::Section:
			collectFieldNames(row.children, names);
			break;
		case SpecKind::Static:
			break;
		default:
			if (std::ranges::find(names, row.source) == names.end()) {
				names.push_back(row.source);
			}
			break;
		}
	}
}

// ---------------------------------------------------------------------------------------
// Building the device tree
// ---------------------------------------------------------------------------------------

struct BuildContext
{
	const QuerySelection &selection;
	const MetricValues &values;
	const QueryStatics &statics;
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

[[nodiscard]] std::optional<std::string> mebibytesOf(const std::optional<std::string> &bytesText)
{
	if (!bytesText) {
		return std::nullopt;
	}
	uint64_t bytes = 0;
	const auto *end = bytesText->data() + bytesText->size();
	const auto [stop, error] = std::from_chars(bytesText->data(), end, bytes);
	if (error != std::errc{} || stop != end) {
		return std::nullopt;
	}
	constexpr double bytesPerMebibyte = 1024.0 * 1024.0;
	return xpum::compat::format("{:.2f}", static_cast<double>(bytes) / bytesPerMebibyte);
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
	case SpecKind::BdfFunction:
		return xpum::compat::format("0x{:x}", parts->function);
	default:
		return std::nullopt;
	}
}

/// Build the node for one spec row, or nothing when the selection hides it or it has nothing to show.
[[nodiscard]] std::optional<QueryNode> buildRow(const Spec &spec, const BuildContext &ctx)
{
	switch (spec.kind) {
	case SpecKind::Section: {
		QueryNode section;
		section.label = spec.label;
		section.key = spec.key;
		for (const auto &child : spec.children) {
			if (auto node = buildRow(child, ctx)) {
				section.children.push_back(std::move(*node));
			}
		}
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
		if (spec.negativeIsUnknown && value && value->starts_with('-')) {
			value.reset();
		}
		return leafOf(spec.label, spec.key, std::move(value), field->unit);
	}
	case SpecKind::Static: {
		const auto *known = staticSpecOf(spec.source);
		if (known == nullptr || !shown(known->group, ctx.selection) || !ctx.statics.has(spec.source)) {
			return std::nullopt;
		}
		if (spec.source == ALLOCATION_SIZE_KEY) {
			return leafOf(spec.label, spec.key, mebibytesOf(ctx.statics.get(spec.source)), MEBIBYTES);
		}
		return leafOf(spec.label, spec.key, ctx.statics.get(spec.source));
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

bool QueryStatics::has(std::string_view key) const
{
	return std::ranges::any_of(values, [key](const StaticValue &entry) { return entry.key == key; });
}

std::optional<std::string> QueryStatics::get(std::string_view key) const
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
		collectFieldNames(reportLayout(), names);
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

void renderNode(const QueryNode &node, std::size_t depth, std::string &out)
{
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
	record.children.push_back(
		leafOf("Used Shared Memory", "used_shared_memory", std::to_string(proc.sharedSize / 1024), mebibytes));
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
						  const QueryStatics &statics, std::span<const psInfo> procs)
{
	QueryNode device;
	device.label = "GPU " + std::string{bdf};
	device.key = "pci_bus_id";
	device.value = std::string{bdf};

	const BuildContext ctx{selection, metricValues, statics};
	for (const auto &row : reportLayout()) {
		if (auto node = buildRow(row, ctx)) {
			device.children.push_back(std::move(*node));
		}
	}
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
	device.children.push_back(leafOf("Recovery Action", "recovery_action", "Firmware update and GPU reset"));
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
[[nodiscard]] QueryStatics gatherStatics(devInfo &dev, const QuerySelection &selection)
{
	QueryStatics statics;
	cmdDiscovery discovery;
	for (const auto &spec : STATIC_TABLE) {
		if (!shown(spec.group, selection) || (spec.amcOnly && !dev.dev->hasAmc())) {
			continue;
		}
		if (spec.getter == nullptr) {
			statics.values.push_back(
				{spec.key, std::string{dev.dev->isInSurvMode() ? DEVICE_STATE_SURV_MODE : DEVICE_STATE_NORMAL}});
			continue;
		}
		std::string line;
		const ze_result_t result = (discovery.*spec.getter)(&dev, &line);
		statics.values.push_back({spec.key, result == ZE_RESULT_SUCCESS ? std::optional{line} : std::nullopt});
	}
	return statics;
}

/// Run each field's getter for one device; a getter that fails reads as absent.
[[nodiscard]] MetricValues evaluate(std::span<const metrics::QueryMetric *const> fields, devInfo &dev,
									const metrics::MetricCache &cache)
{
	MetricValues values;
	values.reserve(fields.size());
	for (const auto *field : fields) {
		metrics::MetricValue out;
		values[field->name] = field->getter(dev, out, cache) == ZE_RESULT_SUCCESS ? std::optional{out} : std::nullopt;
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
		  fields(selectedMetrics(opts.selection)), inputs(metrics::inputsOf(fields)), caches(devices.size()),
		  statics(devices.size())
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
	metrics::MetricInput inputs;
	std::vector<metrics::MetricCache> caches;
	std::vector<QueryStatics> statics;
	bool firstFrame = true;
};

QueryNode QuerySession::sampleDevice(std::size_t index)
{
	devInfo &dev = devices[index];
	metrics::MetricCache &cache = caches[index];
	const bool windowed = inputs != metrics::MetricInput::NONE;
	const bool first = firstFrame;

	auto windowEnd = std::chrono::steady_clock::time_point{};
	if (windowed) {
		if (first) {
			cache = metrics::populateMetricCacheBegin(dev, inputs);
			windowEnd = std::chrono::steady_clock::now() + metrics::detail::SAMPLE_WINDOW;
		} else {
			cache = metrics::populateMetricCacheContinuous(dev, cache, inputs);
		}
	}

	// Work that does not depend on the window runs while it elapses.
	if (first) {
		statics[index] = gatherStatics(dev, options.selection);
	}
	std::vector<psInfo> procs;
	if (options.selection.processes) {
		if (const auto result = cmdPs::getProcessList(&dev, procs); isFatalPsError(result)) {
			DBG("Could not list processes on device {}: 0x{:X}\n", dev.index, static_cast<uint32_t>(result));
			procs.clear();
		}
	}

	if (windowed && first) {
		std::this_thread::sleep_until(windowEnd);
		metrics::populateMetricCacheEnd(dev, cache);
	}
	return buildDeviceNode(devPciAddr(dev), options.selection, evaluate(fields, dev, cache), statics[index], procs);
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
constexpr std::size_t SECTION_LIST_WIDTH = 56;
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

} // namespace

void cmdQuery::help(HELP helpType)
{
	TRACING();
	std::vector<helpCmd> helpList;

	helpList.emplace_back(TITLE, "Print a full report of every GPU attribute");
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Usage: %s query [Options]", progName.c_str());
	helpList.emplace_back(HEADING, "%s query --display MEMORY,ECC --id 0", progName.c_str());
	helpList.emplace_back(HEADING, "%s query --json", progName.c_str());
	helpList.emplace_back(HEADING, "%s query --loop 5 --count 3 --file report.txt", progName.c_str());
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Options:");
	helpList.emplace_back(HEADING, "-h,--help                   Print this help message and exit");
	helpList.emplace_back(HEADING, "-j,--json                   Print the report as JSON");
	helpList.emplace_back(HEADING, "-d,--device,--id <spec>     Device index or PCI BDF address, comma-separated "
								   "(default: all)");
	helpList.emplace_back(HEADING, "--display <sections>        Comma-separated sections to print (default: all):");
	for (const auto &line : wrapSectionNames(SECTION_LIST_WIDTH)) {
		helpList.emplace_back(SUB_HEADING, "%s", line.c_str());
	}
	helpList.emplace_back(HEADING, "--loop <sec>                Print the report every <sec> seconds");
	helpList.emplace_back(HEADING, "--loop-ms <ms>              Print the report every <ms> milliseconds "
								   "(overrides --loop)");
	helpList.emplace_back(HEADING, "--count <n>                 Number of reports (default: unlimited with --loop)");
	helpList.emplace_back(HEADING, "-f,--file <path>            Write the report to <path> instead of stdout");
	helpList.emplace_back(BLANK);
	helpList.emplace_back(TITLE, "Notes:");
	helpList.emplace_back(HEADING, "A value the device cannot supply is printed as N/A.");
	helpList.emplace_back(HEADING, "UTILIZATION, EU_ARRAY and ALL sample the EU array, which takes one extra "
								   "measurement window.");
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

	CLI::App sub{"Print a full report of every GPU attribute", "query"};
	sub.set_help_flag("-h,--help", "Print this help message and exit");
	sub.add_flag("-j,--json", opts.json, "Print the report as JSON");
	sub.add_option("-d,--device,--id", opts.deviceSpec,
				   "Device index or PCI BDF address, comma-separated (default: all)");
	sub.add_option("--display", display, "Comma-separated sections to print (default: all)");
	sub.add_option("--loop", loopSeconds, "Print the report every <sec> seconds")
		->check(CLI::Range(1, std::numeric_limits<int>::max()));
	sub.add_option("--loop-ms", loopMilliseconds, "Print the report every <ms> milliseconds")
		->check(CLI::Range(1, std::numeric_limits<int>::max()));
	sub.add_option("--count", opts.count, "Number of reports")->check(CLI::Range(1, std::numeric_limits<int>::max()));
	sub.add_option("-f,--file", opts.file, "Write the report to a file instead of stdout");

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
	FrameLoopOptions loop{.interval = opts.loop, .count = opts.count, .tty = stdoutIsTerminal() && !opts.file};
	if (opts.file) {
		file.open(*opts.file, std::ios::out | std::ios::trunc);
		if (!file.is_open()) {
			ERR("Failed to open file '{}'\n", *opts.file);
			return static_cast<int>(ZE_RESULT_ERROR_UNKNOWN);
		}
		loop.write = [&file](std::string_view text) { file << text << std::flush; };
	}

	query::QuerySession session{args, std::move(devices), std::move(survivability), opts};
	const int rc = runFrameLoop([&session] { return session.renderFrame(); }, loop);
	if (opts.file && !file) {
		ERR("Failed to write file '{}'\n", *opts.file);
		return rc != 0 ? rc : static_cast<int>(ZE_RESULT_ERROR_UNKNOWN);
	}
	return rc;
}
