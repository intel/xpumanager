/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 * Memory metrics: total/used/free capacity and read/write bandwidth.
 */

#include "memory.h"
#include "device.h"
#include "metrics_registry.h"
#include "ze_api.h"
#include <cstdint>
#include <memory.h>
#include <array>
#include <format>
#include <span>
#include <string>

namespace metrics::memory {

namespace {

constexpr auto total = QueryMetric{// NOLINT(readability-identifier-naming)
								   .name = "memory.total",
								   .unit = "MiB",
								   .description = "Total GPU memory",
								   .source = MetricSource::Live,
								   .groups = MetricGroup::MEMORY,
								   .getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
									   auto *mem = d.dev->getMemory();
									   if (mem == nullptr) {
										   return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
									   }
									   auto size = uint64_t{0};
									   auto const r = mem->getMemorySize(&size);
									   if (r == ZE_RESULT_SUCCESS) {
										   out = std::format("{}", static_cast<double>(size) / (1024.0 * 1024.0));
									   }
									   return r;
								   }};

constexpr auto used = QueryMetric{// NOLINT(readability-identifier-naming)
								  .name = "memory.used",
								  .unit = "MiB",
								  .description = "Used GPU memory",
								  .source = MetricSource::Live,
								  .groups = MetricGroup::MEMORY,
								  .getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
									  auto *mem = d.dev->getMemory();
									  if (mem == nullptr) {
										  return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
									  }
									  auto usedBytes = uint64_t{0};
									  auto const r = mem->getMemoryUsed(&usedBytes, nullptr);
									  if (r == ZE_RESULT_SUCCESS) {
										  out = std::format("{}", static_cast<double>(usedBytes) / (1024.0 * 1024.0));
									  }
									  return r;
								  }};

constexpr auto FREE =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "memory.free",
				.unit = "MiB",
				.description = "Free GPU memory",
				.source = MetricSource::Live,
				.groups = MetricGroup::MEMORY,
				.getter = [](devInfo &d, MetricValue &out, const MetricCache &) -> ze_result_t {
					auto *mem = d.dev->getMemory();
					if (mem == nullptr) {
						return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
					}
					auto totalBytes = uint64_t{0};
					uint64_t usedBytes = 0;
					auto r = mem->getMemorySize(&totalBytes);
					if (r != ZE_RESULT_SUCCESS) {
						return r;
					}
					r = mem->getMemoryUsed(&usedBytes, nullptr);
					if (r != ZE_RESULT_SUCCESS) {
						return r;
					}
					const auto freeBytes = static_cast<int64_t>(totalBytes) - static_cast<int64_t>(usedBytes);
					out = std::format("{}", static_cast<double>(freeBytes > 0 ? freeBytes : 0) / (1024.0 * 1024.0));
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto READ_BANDWIDTH =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "memory.read.bandwidth",
				.unit = "kB/s",
				.description = "GPU memory read bandwidth (Intel-only; dump -m 6)",
				.source = MetricSource::Live,
				.groups = MetricGroup::MEMORY,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					if (!cache.memAvail || cache.memAfter.ts <= cache.memBefore.ts) {
						out = "N/A";
						return ZE_RESULT_SUCCESS;
					}
					const auto dt = static_cast<double>(cache.memAfter.ts - cache.memBefore.ts);
					const auto rd = static_cast<double>(cache.memAfter.read - cache.memBefore.read);
					out = std::format("{}", 1000000.0 * rd / dt / 1024.0);
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto WRITE_BANDWIDTH =
	QueryMetric{// NOLINT(readability-identifier-naming)
				.name = "memory.write.bandwidth",
				.unit = "kB/s",
				.description = "GPU memory write bandwidth (Intel-only; dump -m 7)",
				.source = MetricSource::Live,
				.groups = MetricGroup::MEMORY,
				.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
					if (!cache.memAvail || cache.memAfter.ts <= cache.memBefore.ts) {
						out = "N/A";
						return ZE_RESULT_SUCCESS;
					}
					const auto dt = static_cast<double>(cache.memAfter.ts - cache.memBefore.ts);
					const auto wr = static_cast<double>(cache.memAfter.write - cache.memBefore.write);
					out = std::format("{}", 1000000.0 * wr / dt / 1024.0);
					return ZE_RESULT_SUCCESS;
				}};

constexpr auto BANDWIDTH_UTILIZATION = QueryMetric{
	// NOLINT(readability-identifier-naming)
	.name = "memory.bandwidth.utilization",
	.unit = "%",
	.description = "GPU memory bandwidth utilization as % of peak (Intel-only; dump -m 17)",
	.source = MetricSource::Live,
	.groups = MetricGroup::MEMORY,
	.getter = [](devInfo & /*d*/, MetricValue &out, const MetricCache &cache) -> ze_result_t {
		if (!cache.memAvail || cache.memAfter.ts <= cache.memBefore.ts || cache.memMaxBandwidth == 0) {
			out = "N/A";
			return ZE_RESULT_SUCCESS;
		}
		const auto dt = static_cast<double>(cache.memAfter.ts - cache.memBefore.ts);
		const auto deltaBytes = static_cast<double>(cache.memAfter.read + cache.memAfter.write) -
								static_cast<double>(cache.memBefore.read + cache.memBefore.write);
		out = std::format("{}", 100.0 * deltaBytes * 1.0e6 / dt / static_cast<double>(cache.memMaxBandwidth));
		return ZE_RESULT_SUCCESS;
	}};

constexpr auto ALL =
	std::to_array<QueryMetric>({total, used, FREE, READ_BANDWIDTH, WRITE_BANDWIDTH, BANDWIDTH_UTILIZATION});

} // namespace

std::span<const QueryMetric> getMemoryMetrics() noexcept { return ALL; }

} // namespace metrics::memory
