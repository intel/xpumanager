/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Identity metrics: device name, index, UUID, serial, driver/firmware versions,
 * and the PCI identity fields that carry both IDENTITY and PCI group membership.
 */

#pragma once

#include "device.h"
#include "metrics_registry.h"
#include "ze_api.h"
#include "zes_api.h"
#include <cstdint>
#include <firmware.h>
#include <pci.h>
#include <array>
#include <chrono>
#include <cstring>
#include "utility/compat/format.h"
#include <span>
#include <string>
#include <string_view>

namespace metrics::identity {

/** Width of a BDF address rendered as "dddd:bb:dd.f" (see pci::getBDFStr). */
inline constexpr int BDF_DISPLAY_WIDTH = sizeof("0000:00:00.0") - 1;

inline std::span<const QueryMetric> getIdentityMetrics() noexcept
{
	static constexpr auto nameAliases = std::to_array<std::string_view>({"gpu_name"});
	static constexpr auto indexAliases = std::to_array<std::string_view>({"gpu_index"});
	static constexpr auto uuidAliases = std::to_array<std::string_view>({"gpu_uuid"});
	static constexpr auto serialAliases = std::to_array<std::string_view>({"gpu_serial"});
	static constexpr auto pciBusIdAliases = std::to_array<std::string_view>({"gpu_bus_id"});
	static constexpr auto metrics = std::to_array<QueryMetric>({
		{
			.name = "timestamp",
			.unit = "",
			.description = "The timestamp of when the query was made, in format 'YYYY/MM/DD HH:MM:SS.msec'.",
			.source = MetricSource::Live,
			.groups = MetricGroup::IDENTITY,
			.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &) -> ze_result_t {
				out = xpum::compat::format("{:%Y/%m/%d %H:%M:%S}", std::chrono::floor<std::chrono::milliseconds>(
																	   std::chrono::system_clock::now()));
				return ZE_RESULT_SUCCESS;
			},
		},
		{
			.name = "name",
			.aliases = nameAliases,
			.unit = "",
			.description = "The official product name of the GPU. This is an alphanumeric string.",
			.source = MetricSource::Static,
			.groups = MetricGroup::IDENTITY,
			.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
				ze_device_properties_t props{};
				props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
				auto const r = d.dev->getDevProps(d.deviceHdl, &props);
				if (r == ZE_RESULT_SUCCESS) {
					out = props.name;
				}
				return r;
			},
		},
		{
			.name = "index",
			.aliases = indexAliases,
			.unit = "",
			.description = "Zero based index of the GPU. Can change at each boot.",
			.source = MetricSource::Static,
			.groups = MetricGroup::IDENTITY,
			.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
				out = xpum::compat::format("{}", d.index);
				return ZE_RESULT_SUCCESS;
			},
		},
		{
			.name = "uuid",
			.aliases = uuidAliases,
			.unit = "",
			.description = "Globally unique immutable identifier of the GPU derived from the SoC UUID. Does not "
						   "correspond to any physical label on the board.",
			.source = MetricSource::Static,
			.groups = MetricGroup::IDENTITY,
			.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
				ze_device_properties_t props{};
				props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
				auto const r = d.dev->getDevProps(d.deviceHdl, &props);
				if (r != ZE_RESULT_SUCCESS) {
					return r;
				}
				static_assert(ZE_MAX_DEVICE_UUID_SIZE == 16, "UUID formatting assumes 16 bytes");
				out = xpum::compat::format(
					"{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:"
					"02x}{:02x}{:02x}",
					props.uuid.id[15], props.uuid.id[14], props.uuid.id[13], props.uuid.id[12], props.uuid.id[11],
					props.uuid.id[10], props.uuid.id[9], props.uuid.id[8], props.uuid.id[7], props.uuid.id[6],
					props.uuid.id[5], props.uuid.id[4], props.uuid.id[3], props.uuid.id[2], props.uuid.id[1],
					props.uuid.id[0]);
				return ZE_RESULT_SUCCESS;
			},
		},
		{
			.name = "serial",
			.aliases = serialAliases,
			.unit = "",
			.description =
				"The serial number physically printed on the board. A globally unique immutable alphanumeric value.",
			.source = MetricSource::Static,
			.groups = MetricGroup::IDENTITY,
			.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
				zes_device_properties_t p{};
				p.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
				auto const r = d.dev->zesGetDevProps(d.zesDeviceHdl, &p);
				if (r == ZE_RESULT_SUCCESS) {
					out = p.serialNumber;
				}
				return r;
			},
		},
		{
			.name = "driver_version",
			.unit = "",
			.description = "The version of the installed GPU driver.",
			.source = MetricSource::Static,
			.groups = MetricGroup::IDENTITY,
			.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
				ze_driver_properties_t props{};
				auto const r = d.dev->getDriverProperties(&props);
				if (r == ZE_RESULT_SUCCESS) {
					out = std::to_string(props.driverVersion);
				}
				return r;
			},
		},
		{
			.name = "vbios_version",
			.unit = "",
			.description = "The version of the GFX firmware currently running on the GPU board.",
			.source = MetricSource::Static,
			.groups = MetricGroup::IDENTITY,
			.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
				auto *fw = d.dev->getFirmware();
				if (fw == nullptr) {
					return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
				}
				auto *p = d.dev->getPCI();
				if (p == nullptr) {
					return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
				}
				std::array<char, 256> ver{};
				auto const r = fw->getFWversion(fwType::GFX, p->getBDFStr().c_str(), ver.data(),
												static_cast<uint32_t>(ver.size()));
				if (r == ZE_RESULT_SUCCESS) {
					out = std::string(ver.data(), strnlen(ver.data(), ver.size()));
				}
				return r;
			},
		},
		{
			.name = "pci.bus_id",
			.aliases = pciBusIdAliases,
			.unit = "",
			.description = "PCI bus address as 'domain:bus:device.function', in hex.",
			.source = MetricSource::Static,
			.groups = MetricGroup::IDENTITY | MetricGroup::PCI,
			.minWidth = BDF_DISPLAY_WIDTH,
			.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
				auto *p = d.dev->getPCI();
				if (p == nullptr) {
					return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
				}
				out = p->getBDFStr();
				return ZE_RESULT_SUCCESS;
			},
		},
		{
			.name = "pci.device_id",
			.unit = "",
			.description = "PCI device id, in hex.",
			.source = MetricSource::Static,
			.groups = MetricGroup::IDENTITY | MetricGroup::PCI,
			.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
				ze_device_properties_t props{};
				props.stype = ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES;
				auto const r = d.dev->getDevProps(d.deviceHdl, &props);
				if (r == ZE_RESULT_SUCCESS) {
					out = xpum::compat::format("0x{:04X}", props.deviceId);
				}
				return r;
			},
		},
		{
			.name = "pci.sub_device_id",
			.unit = "",
			.description = "Board number as reported by the Sysman device properties (used as the PCI sub-device slot "
						   "identifier).",
			.source = MetricSource::Static,
			.groups = MetricGroup::IDENTITY | MetricGroup::PCI,
			.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
				zes_device_properties_t p{};
				p.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
				auto const r = d.dev->zesGetDevProps(d.zesDeviceHdl, &p);
				if (r == ZE_RESULT_SUCCESS) {
					out = p.boardNumber;
				}
				return r;
			},
		},
	});
	return metrics;
}

} // namespace metrics::identity
